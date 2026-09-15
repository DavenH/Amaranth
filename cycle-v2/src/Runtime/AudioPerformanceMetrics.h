#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstdint>

#include "Runtime/PerformanceDistribution.h"

namespace CycleV2 {

class AudioPerformanceMetrics final {
public:
    enum class Stage : uint8_t {
        GraphAdoption,
        OutputClear,
        BlockSetup,
        MidiScheduling,
        VoiceRendering,
        GlobalRendering,
        OutputConditioning,
        MeterPublication,
        LiveCapture,
        Count
    };

    static constexpr size_t stageCount = static_cast<size_t>(Stage::Count);
    static constexpr size_t queueCapacity = 256;

    struct RealtimeSample {
        uint64_t generation {};
        uint64_t callbackStartMicroseconds {};
        uint64_t callbackDurationMicroseconds {};
        uint64_t deadlineMicroseconds {};
        uint64_t graphRevision {};
        uint32_t frameCount {};
        uint32_t executionStepCount {};
        uint16_t activeVoiceCount {};
        uint16_t scheduledMidiEventCount {};
        uint32_t executionStepVisitCount {};
        uint32_t modulationBindingVisitCount {};
        uint32_t spectralTransferBindingVisitCount {};
        std::array<uint64_t, stageCount> stageDurations {};
    };

    struct Snapshot {
        bool enabled {};
        uint64_t elapsedMicroseconds {};
        uint64_t telemetryDrops {};
        uint64_t deadlineOverruns {};
        uint64_t totalFrames {};
        uint64_t totalVoiceBlocks {};
        uint64_t totalExecutionStepVisits {};
        uint64_t totalModulationBindingVisits {};
        uint64_t totalSpectralTransferBindingVisits {};
        uint64_t latestGraphRevision {};
        uint32_t maximumFrameCount {};
        uint16_t maximumActiveVoiceCount {};
        uint16_t maximumScheduledMidiEventCount {};
        uint32_t maximumExecutionStepCount {};
        PerformanceDistribution callbackDuration;
        PerformanceDistribution deadlineUtilizationPermille;
        std::array<PerformanceDistribution, stageCount> stages;
    };

    class ScopedRealtimeStage final {
    public:
        ScopedRealtimeStage(RealtimeSample* sample, Stage stage) noexcept;
        ~ScopedRealtimeStage();

        ScopedRealtimeStage(const ScopedRealtimeStage&) = delete;
        ScopedRealtimeStage& operator=(const ScopedRealtimeStage&) = delete;

    private:
        RealtimeSample* measuredSample;
        Stage measuredStage;
        uint64_t startMicroseconds;
    };

    bool beginRealtimeSample(
            RealtimeSample& sample,
            int frameCount,
            double sampleRate) const noexcept;
    void publishRealtimeSample(RealtimeSample sample) noexcept;
    void serviceNonRealtime();
    void resetAndEnable();
    void disable() noexcept;
    Snapshot snapshot() const;
    juce::var toVar() const;

    static uint64_t timestampMicroseconds() noexcept;
    static void finishStage(
            RealtimeSample& sample,
            Stage stage,
            uint64_t startMicroseconds) noexcept;
    static const char* label(Stage stage);

private:
    struct Aggregate {
        uint64_t deadlineOverruns {};
        uint64_t totalFrames {};
        uint64_t totalVoiceBlocks {};
        uint64_t totalExecutionStepVisits {};
        uint64_t totalModulationBindingVisits {};
        uint64_t totalSpectralTransferBindingVisits {};
        uint64_t latestGraphRevision {};
        uint32_t maximumFrameCount {};
        uint16_t maximumActiveVoiceCount {};
        uint16_t maximumScheduledMidiEventCount {};
        uint32_t maximumExecutionStepCount {};
        PerformanceDistribution callbackDuration;
        PerformanceDistribution deadlineUtilizationPermille;
        std::array<PerformanceDistribution, stageCount> stages;
    };

    void aggregate(const RealtimeSample& sample);

    std::array<RealtimeSample, queueCapacity> samples;
    std::atomic<size_t> writePosition {};
    std::atomic<size_t> readPosition {};
    std::atomic<uint64_t> droppedSamples {};
    std::atomic<uint64_t> generation { 1 };
    std::atomic<bool> enabled {};

    mutable juce::CriticalSection aggregateLock;
    Aggregate aggregateData;
    uint64_t windowStartMicroseconds { timestampMicroseconds() };
    uint64_t droppedSamplesAtReset {};
};

}
