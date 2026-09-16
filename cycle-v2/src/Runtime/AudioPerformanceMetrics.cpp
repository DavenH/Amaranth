#include "Runtime/AudioPerformanceMetrics.h"

#include <algorithm>

namespace CycleV2 {

using namespace juce;

static_assert(std::atomic<size_t>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

namespace {

size_t indexFor(AudioPerformanceMetrics::Stage stage) {
    return static_cast<size_t>(stage);
}

var sourcePerformanceToVar(
        const CycleDsp::SourceRenderPerformance& counts,
        uint64_t callbackCount) {
    static constexpr std::array<const char*, CycleDsp::SourceRenderPerformance::stageCount>
            labels { "morphResolution", "rasterization", "sampling", "gain", "stereoCopy" };
    auto* root = new DynamicObject();
    root->setProperty("waveformSegments", (int64) counts.waveform.waveformSegments);
    root->setProperty("integralSegments", (int64) counts.waveform.integralSegments);
    for (size_t index = 0; index < labels.size(); ++index) {
        auto* stage = new DynamicObject();
        stage->setProperty("totalNanoseconds", (int64) counts.nanoseconds[index]);
        stage->setProperty("totalOperations", (int64) counts.operations[index]);
        stage->setProperty("meanMs", callbackCount == 0 ? 0.0
                : (double) counts.nanoseconds[index] / (double) callbackCount / 1.0e6);
        stage->setProperty("meanOperationMicroseconds", counts.operations[index] == 0 ? 0.0
                : (double) counts.nanoseconds[index] / (double) counts.operations[index] / 1000.0);
        root->setProperty(labels[index], var(stage));
    }
    return var(root);
}

var utilizationToVar(const PerformanceDistribution& distribution) {
    auto* object = new DynamicObject();
    object->setProperty("count", (int64) distribution.count);
    object->setProperty(
            "meanPercent",
            distribution.count == 0
                    ? 0.0
                    : (double) distribution.totalMicroseconds
                            / (double) distribution.count / 10.0);
    object->setProperty(
            "p50Percent",
            performancePercentileValue(distribution, 0.50) / 10.0);
    object->setProperty(
            "p95Percent",
            performancePercentileValue(distribution, 0.95) / 10.0);
    object->setProperty(
            "p99Percent",
            performancePercentileValue(distribution, 0.99) / 10.0);
    object->setProperty(
            "maxPercent",
            (double) distribution.maximumMicroseconds / 10.0);
    return var(object);
}

}

AudioPerformanceMetrics::ScopedRealtimeStage::ScopedRealtimeStage(
        RealtimeSample* sample,
        Stage stage) noexcept :
        measuredSample(sample)
    ,   measuredStage(stage)
    ,   startMicroseconds(sample == nullptr ? 0 : timestampMicroseconds()) {
}

AudioPerformanceMetrics::ScopedRealtimeStage::~ScopedRealtimeStage() {
    if (measuredSample != nullptr) {
        finishStage(*measuredSample, measuredStage, startMicroseconds);
    }
}

AudioPerformanceMetrics::ScopedOscillatorRecipeStage::ScopedOscillatorRecipeStage(
        OscillatorRegionPerformanceCounts* counts,
        OscillatorRecipeStage stage) noexcept
        : measuredCounts(counts),
          measuredStageIndex(static_cast<size_t>(stage)),
          startMicroseconds(counts == nullptr ? 0 : timestampMicroseconds()) {
}

AudioPerformanceMetrics::ScopedOscillatorRecipeStage::~ScopedOscillatorRecipeStage() {
    if (measuredCounts == nullptr) {
        return;
    }
    measuredCounts->recipeStageDurations[measuredStageIndex]
            += timestampMicroseconds() - startMicroseconds;
    ++measuredCounts->recipeStageOperationCounts[measuredStageIndex];
}

bool AudioPerformanceMetrics::beginRealtimeSample(
        RealtimeSample& sample,
        int frameCount,
        double sampleRate) const noexcept {
    if (!enabled.load(std::memory_order_relaxed)) {
        return false;
    }

    sample = {};
    sample.generation = generation.load(std::memory_order_acquire);
    sample.callbackStartMicroseconds = timestampMicroseconds();
    sample.frameCount = (uint32_t) jmax(0, frameCount);
    if (frameCount > 0 && sampleRate > 0.0) {
        sample.deadlineMicroseconds = (uint64_t) (
                (double) frameCount * 1000000.0 / sampleRate);
    }
    return true;
}

void AudioPerformanceMetrics::publishRealtimeSample(
        RealtimeSample sample) noexcept {
    if (sample.callbackDurationMicroseconds == 0) {
        sample.callbackDurationMicroseconds = timestampMicroseconds()
                - sample.callbackStartMicroseconds;
    }
    const size_t write = writePosition.load(std::memory_order_relaxed);
    const size_t next = (write + 1) % samples.size();
    if (next == readPosition.load(std::memory_order_acquire)) {
        droppedSamples.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    samples[write] = sample;
    writePosition.store(next, std::memory_order_release);
}

void AudioPerformanceMetrics::serviceNonRealtime() {
    const ScopedLock lock(aggregateLock);
    size_t read = readPosition.load(std::memory_order_relaxed);
    const size_t write = writePosition.load(std::memory_order_acquire);
    const uint64_t activeGeneration = generation.load(std::memory_order_acquire);
    while (read != write) {
        const RealtimeSample& sample = samples[read];
        if (sample.generation == activeGeneration) {
            aggregate(sample);
        }
        read = (read + 1) % samples.size();
    }
    readPosition.store(read, std::memory_order_release);
}

void AudioPerformanceMetrics::resetAndEnable() {
    const ScopedLock lock(aggregateLock);
    generation.fetch_add(1, std::memory_order_acq_rel);
    aggregateData = {};
    windowStartMicroseconds = timestampMicroseconds();
    droppedSamplesAtReset = droppedSamples.load(std::memory_order_acquire);
    enabled.store(true, std::memory_order_release);
}

void AudioPerformanceMetrics::disable() noexcept {
    enabled.store(false, std::memory_order_release);
}

AudioPerformanceMetrics::Snapshot AudioPerformanceMetrics::snapshot() const {
    const uint64_t timestamp = timestampMicroseconds();
    const ScopedLock lock(aggregateLock);
    return {
            enabled.load(std::memory_order_acquire),
            timestamp - windowStartMicroseconds,
            droppedSamples.load(std::memory_order_acquire) - droppedSamplesAtReset,
            aggregateData.deadlineOverruns,
            aggregateData.totalFrames,
            aggregateData.totalVoiceBlocks,
            aggregateData.totalExecutionStepVisits,
            aggregateData.totalModulationBindingVisits,
            aggregateData.totalSpectralTransferBindingVisits,
            aggregateData.totalContextPatches,
            aggregateData.totalOscillatorRegionRenders,
            aggregateData.totalOscillatorRecipeRenders,
            aggregateData.totalOscillatorLaneCycles,
            aggregateData.totalOscillatorMixedLanes,
            aggregateData.totalDequeuedMidiEvents,
            aggregateData.totalSortedMidiItems,
            aggregateData.totalCompactedMidiItems,
            aggregateData.maximumBlockStorageValues,
            aggregateData.maximumGridStorageValues,
            aggregateData.latestGraphRevision,
            aggregateData.maximumFrameCount,
            aggregateData.maximumActiveVoiceCount,
            aggregateData.maximumScheduledMidiEventCount,
            aggregateData.maximumExecutionStepCount,
            aggregateData.callbackDuration,
            aggregateData.deadlineUtilizationPermille,
            aggregateData.stages,
            aggregateData.oscillatorStages,
            aggregateData.oscillatorRecipeStages,
            aggregateData.totalOscillatorRecipeStageOperations,
            aggregateData.timeSources,
            aggregateData.spectralSources
        };
}

var AudioPerformanceMetrics::toVar() const {
    const Snapshot current = snapshot();
    auto* root = new DynamicObject();
    root->setProperty("schema", "cycle-v2-audio-performance.v1");
    root->setProperty("enabled", current.enabled);
    root->setProperty("elapsedMs", (double) current.elapsedMicroseconds / 1000.0);
    root->setProperty("callbackCount", (int64) current.callbackDuration.count);
    root->setProperty("telemetryDrops", (int64) current.telemetryDrops);
    root->setProperty("deadlineOverruns", (int64) current.deadlineOverruns);
    root->setProperty(
            "callbackDuration",
            performanceDistributionToVar(current.callbackDuration));
    root->setProperty(
            "deadlineUtilization",
            utilizationToVar(current.deadlineUtilizationPermille));

    auto* workload = new DynamicObject();
    workload->setProperty("totalFrames", (int64) current.totalFrames);
    workload->setProperty("totalVoiceBlocks", (int64) current.totalVoiceBlocks);
    workload->setProperty(
            "totalExecutionStepVisits",
            (int64) current.totalExecutionStepVisits);
    workload->setProperty(
            "meanExecutionStepVisits",
            current.callbackDuration.count == 0
                    ? 0.0
                    : (double) current.totalExecutionStepVisits
                            / (double) current.callbackDuration.count);
    workload->setProperty(
            "totalModulationBindingVisits",
            (int64) current.totalModulationBindingVisits);
    workload->setProperty(
            "meanModulationBindingVisits",
            current.callbackDuration.count == 0
                    ? 0.0
                    : (double) current.totalModulationBindingVisits
                            / (double) current.callbackDuration.count);
    workload->setProperty(
            "totalSpectralTransferBindingVisits",
            (int64) current.totalSpectralTransferBindingVisits);
    workload->setProperty(
            "meanSpectralTransferBindingVisits",
            current.callbackDuration.count == 0
                    ? 0.0
                    : (double) current.totalSpectralTransferBindingVisits
                            / (double) current.callbackDuration.count);
    workload->setProperty("totalContextPatches", (int64) current.totalContextPatches);
    workload->setProperty(
            "meanContextPatches",
            current.callbackDuration.count == 0
                    ? 0.0
                    : (double) current.totalContextPatches
                            / (double) current.callbackDuration.count);
    workload->setProperty(
            "totalOscillatorRegionRenders",
            (int64) current.totalOscillatorRegionRenders);
    workload->setProperty(
            "totalOscillatorRecipeRenders",
            (int64) current.totalOscillatorRecipeRenders);
    workload->setProperty(
            "totalOscillatorLaneCycles",
            (int64) current.totalOscillatorLaneCycles);
    workload->setProperty(
            "totalOscillatorMixedLanes",
            (int64) current.totalOscillatorMixedLanes);
    workload->setProperty(
            "totalDequeuedMidiEvents",
            (int64) current.totalDequeuedMidiEvents);
    workload->setProperty("totalSortedMidiItems", (int64) current.totalSortedMidiItems);
    workload->setProperty(
            "totalCompactedMidiItems",
            (int64) current.totalCompactedMidiItems);
    workload->setProperty(
            "maximumBlockStorageValues",
            (int64) current.maximumBlockStorageValues);
    workload->setProperty(
            "maximumGridStorageValues",
            (int64) current.maximumGridStorageValues);
    workload->setProperty("latestGraphRevision", (int64) current.latestGraphRevision);
    workload->setProperty("maximumFrameCount", (int) current.maximumFrameCount);
    workload->setProperty(
            "maximumActiveVoiceCount",
            (int) current.maximumActiveVoiceCount);
    workload->setProperty(
            "maximumScheduledMidiEventCount",
            (int) current.maximumScheduledMidiEventCount);
    workload->setProperty(
            "maximumExecutionStepCount",
            (int) current.maximumExecutionStepCount);
    root->setProperty("workload", var(workload));

    auto* stages = new DynamicObject();
    for (size_t index = 0; index < stageCount; ++index) {
        const Stage stage = static_cast<Stage>(index);
        stages->setProperty(
                label(stage),
                performanceDistributionToVar(current.stages[index]));
    }
    root->setProperty("stages", var(stages));

    auto* oscillatorStages = new DynamicObject();
    for (size_t index = 0; index < oscillatorStageCount; ++index) {
        const OscillatorStage stage = static_cast<OscillatorStage>(index);
        oscillatorStages->setProperty(
                label(stage),
                performanceDistributionToVar(current.oscillatorStages[index]));
    }
    root->setProperty("oscillatorStages", var(oscillatorStages));

    auto* oscillatorRecipeStages = new DynamicObject();
    for (size_t index = 0; index < oscillatorRecipeStageCount; ++index) {
        const OscillatorRecipeStage stage
                = static_cast<OscillatorRecipeStage>(index);
        var stageValue = performanceDistributionToVar(
                current.oscillatorRecipeStages[index]);
        auto* stageObject = stageValue.getDynamicObject();
        stageObject->setProperty(
                "totalOperations",
                (int64) current.totalOscillatorRecipeStageOperations[index]);
        stageObject->setProperty(
                "meanOperations",
                current.callbackDuration.count == 0
                        ? 0.0
                        : (double) current.totalOscillatorRecipeStageOperations[index]
                                / (double) current.callbackDuration.count);
        oscillatorRecipeStages->setProperty(label(stage), stageValue);
    }
    root->setProperty("oscillatorRecipeStages", var(oscillatorRecipeStages));
    auto* sources = new DynamicObject();
    sources->setProperty("time", sourcePerformanceToVar(
            current.timeSources, current.callbackDuration.count));
    sources->setProperty("spectral", sourcePerformanceToVar(
            current.spectralSources, current.callbackDuration.count));
    root->setProperty("oscillatorSourceStages", var(sources));
    return var(root);
}

uint64_t AudioPerformanceMetrics::timestampMicroseconds() noexcept {
    return (uint64_t) (Time::getMillisecondCounterHiRes() * 1000.0);
}

void AudioPerformanceMetrics::finishStage(
        RealtimeSample& sample,
        Stage stage,
        uint64_t startMicroseconds) noexcept {
    sample.stageDurations[indexFor(stage)] = timestampMicroseconds()
            - startMicroseconds;
}

const char* AudioPerformanceMetrics::label(Stage stage) {
    switch (stage) {
        case Stage::GraphAdoption:       return "graphAdoption";
        case Stage::OutputClear:         return "outputClear";
        case Stage::BlockSetup:          return "blockSetup";
        case Stage::MidiScheduling:      return "midiScheduling";
        case Stage::VoiceRendering:      return "voiceRendering";
        case Stage::GlobalRendering:     return "globalRendering";
        case Stage::OutputConditioning:  return "outputConditioning";
        case Stage::MeterPublication:    return "meterPublication";
        case Stage::LiveCapture:         return "liveCapture";
        case Stage::Count:               break;
    }
    return "unknown";
}

const char* AudioPerformanceMetrics::label(OscillatorStage stage) {
    switch (stage) {
        case OscillatorStage::RegionRendering:  return "regionRendering";
        case OscillatorStage::RecipeRendering:  return "recipeRendering";
        case OscillatorStage::LaneRendering:    return "laneRendering";
        case OscillatorStage::OutputMixing:     return "outputMixing";
        case OscillatorStage::Count:            break;
    }
    return "unknown";
}

const char* AudioPerformanceMetrics::label(OscillatorRecipeStage stage) {
    switch (stage) {
        case OscillatorRecipeStage::TimeSourceRendering:
            return "timeSourceRendering";
        case OscillatorRecipeStage::SpectralSourceRendering:
            return "spectralSourceRendering";
        case OscillatorRecipeStage::ForwardTransform:
            return "forwardTransform";
        case OscillatorRecipeStage::InverseTransform:
            return "inverseTransform";
        case OscillatorRecipeStage::GraphCombining:
            return "graphCombining";
        case OscillatorRecipeStage::Count:
            break;
    }
    return "unknown";
}

void AudioPerformanceMetrics::aggregate(const RealtimeSample& sample) {
    aggregateData.timeSources.add(sample.timeSources);
    aggregateData.spectralSources.add(sample.spectralSources);
    recordPerformanceSample(
            aggregateData.callbackDuration,
            sample.callbackDurationMicroseconds);
    if (sample.deadlineMicroseconds > 0) {
        const uint64_t utilizationPermille = sample.callbackDurationMicroseconds * 1000
                / sample.deadlineMicroseconds;
        recordPerformanceSample(
                aggregateData.deadlineUtilizationPermille,
                utilizationPermille);
        if (sample.callbackDurationMicroseconds > sample.deadlineMicroseconds) {
            ++aggregateData.deadlineOverruns;
        }
    }
    aggregateData.totalFrames += sample.frameCount;
    aggregateData.totalVoiceBlocks += sample.activeVoiceCount;
    aggregateData.totalExecutionStepVisits += sample.executionStepVisitCount;
    aggregateData.totalModulationBindingVisits += sample.modulationBindingVisitCount;
    aggregateData.totalSpectralTransferBindingVisits
            += sample.spectralTransferBindingVisitCount;
    aggregateData.totalContextPatches += sample.contextPatchCount;
    aggregateData.totalOscillatorRegionRenders
            += sample.oscillatorRegionRenderCount;
    aggregateData.totalOscillatorRecipeRenders
            += sample.oscillatorRecipeRenderCount;
    aggregateData.totalOscillatorLaneCycles += sample.oscillatorLaneCycleCount;
    aggregateData.totalOscillatorMixedLanes += sample.oscillatorMixedLaneCount;
    aggregateData.totalDequeuedMidiEvents += sample.dequeuedMidiEventCount;
    aggregateData.totalSortedMidiItems += sample.sortedMidiItemCount;
    aggregateData.totalCompactedMidiItems += sample.compactedMidiItemCount;
    aggregateData.maximumBlockStorageValues = std::max(
            aggregateData.maximumBlockStorageValues,
            sample.blockStorageValues);
    aggregateData.maximumGridStorageValues = std::max(
            aggregateData.maximumGridStorageValues,
            sample.gridStorageValues);
    aggregateData.latestGraphRevision = sample.graphRevision;
    aggregateData.maximumFrameCount = std::max(
            aggregateData.maximumFrameCount,
            sample.frameCount);
    aggregateData.maximumActiveVoiceCount = std::max(
            aggregateData.maximumActiveVoiceCount,
            sample.activeVoiceCount);
    aggregateData.maximumScheduledMidiEventCount = std::max(
            aggregateData.maximumScheduledMidiEventCount,
            sample.scheduledMidiEventCount);
    aggregateData.maximumExecutionStepCount = std::max(
            aggregateData.maximumExecutionStepCount,
            sample.executionStepCount);
    for (size_t index = 0; index < stageCount; ++index) {
        if (sample.stageDurations[index] > 0) {
            recordPerformanceSample(
                    aggregateData.stages[index],
                    sample.stageDurations[index]);
        }
    }
    for (size_t index = 0; index < oscillatorStageCount; ++index) {
        if (sample.oscillatorStageDurations[index] > 0) {
            recordPerformanceSample(
                    aggregateData.oscillatorStages[index],
                    sample.oscillatorStageDurations[index]);
        }
    }
    for (size_t index = 0; index < oscillatorRecipeStageCount; ++index) {
        aggregateData.totalOscillatorRecipeStageOperations[index]
                += sample.oscillatorRecipeStageOperationCounts[index];
        if (sample.oscillatorRecipeStageDurations[index] > 0) {
            recordPerformanceSample(
                    aggregateData.oscillatorRecipeStages[index],
                    sample.oscillatorRecipeStageDurations[index]);
        }
    }
}

}
