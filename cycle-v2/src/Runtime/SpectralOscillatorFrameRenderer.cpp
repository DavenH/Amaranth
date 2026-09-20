#include "Runtime/SpectralOscillatorFrameRenderer.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "Runtime/AudioPerformanceMetrics.h"
#include "Runtime/OscillatorRegionPlanView.h"
#include "Runtime/PreparedTrimeshMorphBinding.h"
#include "Runtime/SpectralFrameGraphCombiner.h"

#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>
#include <Util/LogRegionMapping.h>

namespace CycleV2 {

namespace {

bool supportedRole(AudioModuleRole role) {
    return role == AudioModuleRole::MeshSource
            || role == AudioModuleRole::WaveSource
            || role == AudioModuleRole::SpectralLayer
            || role == AudioModuleRole::Fft
            || role == AudioModuleRole::Ifft
            || role == AudioModuleRole::Add
            || role == AudioModuleRole::Multiply;
}

bool sourceRole(AudioModuleRole role) {
    return role == AudioModuleRole::MeshSource
            || role == AudioModuleRole::WaveSource;
}

}

bool SpectralOscillatorFrameRenderer::supports(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    const OscillatorRegionPlanView regionView(plan, region);
    if (region.strategy != OscillatorExecutionStrategy::SharedSpectralFrame
            || !regionView.isValid()) {
        return false;
    }

    for (const int stepIndex : region.stepIndices) {
        if (!supportedRole(plan.steps[(size_t) stepIndex].audioRole)) {
            return false;
        }
    }

    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        if (sourceRole(step.audioRole)) {
            if (!PreparedTrimeshMorphBinding::supports(plan, step)) {
                return false;
            }
            continue;
        }
        const bool leftInRegion = regionView.inputComesFromRegion(step, 0);
        const bool rightInRegion = regionView.inputComesFromRegion(step, 1);
        if (step.audioRole == AudioModuleRole::Add) {
            if (!leftInRegion && !rightInRegion) {
                return false;
            }
            continue;
        }
        if (!leftInRegion) {
            return false;
        }
        if ((step.audioRole == AudioModuleRole::Ifft
                    || step.audioRole == AudioModuleRole::Multiply)
                && !rightInRegion) {
            return false;
        }
    }
    return true;
}

bool SpectralOscillatorFrameRenderer::prepare(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        int maximumFrameSizeToUse,
        const std::vector<NodeAudioProcessor*>& processors,
        int laneCount,
        const String& pitchEnvelopeNodeId) {
    if (!supports(plan, region)
            || !SpectralFrameTransformStage::supportsFrameSize(
                    maximumFrameSizeToUse)) {
        return false;
    }

    maximumFrameSize = maximumFrameSizeToUse;
    if (!cycleEnvelopes.prepare(
            plan,
            region,
            processors,
            laneCount,
            pitchEnvelopeNodeId)) {
        return false;
    }
    slotStride = maximumFrameSize + 2;
    outputSlot = -1;
    hasSpectralMesh = false;
    operations.clear();
    operations.reserve(region.stepIndices.size());
    const OscillatorRegionPlanView regionView(plan, region);
    std::vector<std::array<int, 2>> slotsForStep(
            plan.steps.size(),
            { -1, -1 });
    int slotCount = 0;

    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        Operation operation;
        operation.outputDomain = step.outputs.empty()
                ? PortDomain::TimeSignal
                : step.outputs.front().domain;
        for (size_t outputIndex = 0;
                outputIndex < step.outputs.size() && outputIndex < operation.outputs.size();
                ++outputIndex) {
            operation.outputs[outputIndex] = slotCount++;
            slotsForStep[(size_t) stepIndex][outputIndex] = operation.outputs[outputIndex];
        }

        const auto inputSlot = [&](int portIndex, SpectralMagnitudeTransfer& transfer) {
            const auto* input = regionView.inputForPort(step, portIndex);
            if (input == nullptr) {
                return -1;
            }
            int sourceStepIndex = input->sourceStepIndex;
            int sourceOutputIndex = input->sourceOutputIndex;
            if (input->magnitudeTransfer.isActive()) {
                transfer = resolveSpectralMagnitudeTransfer(
                        plan,
                        input->magnitudeTransfer);
                sourceStepIndex = input->magnitudeTransfer.sourceStepIndex;
                sourceOutputIndex = input->magnitudeTransfer.sourceOutputIndex;
            }
            if (sourceStepIndex < 0
                    || sourceOutputIndex < 0
                    || sourceOutputIndex >= 2) {
                return -1;
            }
            return slotsForStep[(size_t) sourceStepIndex][(size_t) sourceOutputIndex];
        };
        operation.leftInput = inputSlot(0, operation.leftTransfer);
        operation.rightInput = inputSlot(1, operation.rightTransfer);

        if (operation.outputs[0] < 0
                || (step.audioRole == AudioModuleRole::Fft
                        && operation.outputs[1] < 0)
                || (!sourceRole(step.audioRole)
                        && step.audioRole != AudioModuleRole::Add
                        && operation.leftInput < 0)
                || ((step.audioRole == AudioModuleRole::Ifft
                            || step.audioRole == AudioModuleRole::Multiply)
                        && operation.rightInput < 0)) {
            return false;
        }
        if (step.audioRole == AudioModuleRole::Add
                && operation.leftInput < 0
                && operation.rightInput < 0) {
            return false;
        }

        switch (step.audioRole) {
            case AudioModuleRole::MeshSource:
            case AudioModuleRole::WaveSource: {
                operation.source = SpectralFrameSourceRenderer::create(
                        plan, step, maximumFrameSize);
                if (operation.source == nullptr) {
                    return false;
                }
                if (operation.outputDomain == PortDomain::TimeSignal) {
                    operation.type = OperationType::TimeTrimesh;
                } else {
                    operation.type = OperationType::SpectralTrimesh;
                }
                hasSpectralMesh |= operation.source->hasActiveSpectralMesh();
                break;
            }
            case AudioModuleRole::Fft:      operation.type = OperationType::Fft; break;
            case AudioModuleRole::SpectralLayer: {
                const auto configuration = std::dynamic_pointer_cast<
                        const PanConfiguration>(step.configuration.value);
                if (configuration == nullptr) {
                    return false;
                }
                operation.type = OperationType::SpectralLayer;
                operation.pan = configuration->pan;
                operation.multiplicative = configuration->multiplicative;
                break;
            }
            case AudioModuleRole::Ifft:     operation.type = OperationType::Ifft; break;
            case AudioModuleRole::Add:      operation.type = OperationType::Add; break;
            case AudioModuleRole::Multiply: operation.type = OperationType::Multiply; break;
            default: return false;
        }
        operations.push_back(std::move(operation));
    }

    const auto& materialization = slotsForStep[
            (size_t) region.materializationStepIndex];
    outputSlot = materialization[0];
    if (outputSlot < 0 || slotCount <= 0) {
        return false;
    }
    slotMemory.resize(2 * slotCount * slotStride);
    const int maximumBinCount = RealFftFullPolarSpectrum::binCountForBufferSize(
            maximumFrameSize);
    magnitudeScratch.resize(maximumFrameSize);
    phaseScratch.resize(maximumBinCount);
    phaseHarmonicScale.resize(maximumBinCount - 1);
    CycleDsp::SpectralLayerCore::preparePhaseHarmonicScale(
            phaseHarmonicScale.withSize(maximumBinCount - 1));

    if (!transformStage.prepare(maximumFrameSize)) {
        return false;
    }
    reset();
    return true;
}

void SpectralOscillatorFrameRenderer::reset() {
    renderCount = 0;
    lifecycleSeedReady = false;
    cycleEnvelopes.reset();
    for (auto& operation : operations) {
        if (operation.source != nullptr) {
            operation.source->reset();
        }
    }
}

void SpectralOscillatorFrameRenderer::applyLifecycleEvent(
        const NoteLifecycleEvent& event) {
    if (event.type == NoteLifecycleType::NoteOn) {
        lifecycleSeedReady = false;
    }
    cycleEnvelopes.applyLifecycleEvent(event);
}

bool SpectralOscillatorFrameRenderer::renderFrame(
        int frameSize,
        int midiNote,
        Buffer<float> left,
        Buffer<float> right) {
    return renderFrameInternal(
            frameSize,
            midiNote,
            nullptr,
            0,
            0,
            0,
            left,
            right,
            true);
}

bool SpectralOscillatorFrameRenderer::renderFrame(
        int frameSize,
        int midiNote,
        const PreparedOscillatorProcessContext& context,
        size_t blockSampleOffset,
        double voiceSamplePosition,
        size_t elapsedSamples,
        Buffer<float> left,
        Buffer<float> right,
        bool refreshTimeSources) {
    return renderFrameInternal(
            frameSize,
            midiNote,
            &context,
            blockSampleOffset,
            voiceSamplePosition,
            elapsedSamples,
            left,
            right,
            refreshTimeSources);
}

bool SpectralOscillatorFrameRenderer::renderFrameInternal(
        int frameSize,
        int midiNote,
        const PreparedOscillatorProcessContext* context,
        size_t blockSampleOffset,
        double voiceSamplePosition,
        size_t elapsedSamples,
        Buffer<float> left,
        Buffer<float> right,
        bool refreshTimeSources) {
    Transform* transform = transformStage.transformFor(frameSize);
    if (transform == nullptr
            || left.size() != frameSize
            || right.size() != frameSize
            || outputSlot < 0) {
        left.zero();
        right.zero();
        return false;
    }

    prepareFrameRandom(context);
    if (context != nullptr && context->voice != nullptr) {
        cycleEnvelopes.advanceAll(
                (int) elapsedSamples,
                context->voice->controls.normalizedVoiceTimeIncrement);
    }
    CycleDsp::SpectralFrameCapture frameCapture(
            context != nullptr && context->voice != nullptr
                    ? context->voice->spectralStageCapture : nullptr,
            renderCount,
            (uint64_t) voiceSamplePosition,
            midiNote);
    const int activeHarmonicCount = jmin(
            RealFftFullPolarSpectrum::binCountForBufferSize(frameSize) - 1,
            LogRegionMapping(midiNote + LogRegionMapping::legacyMidiNoteBias).regionSize());
    const SpectralFrameGraphCombiner graphCombiner(
            frameCapture, activeHarmonicCount);
    for (auto& operation : operations) {
        const auto performanceStage = [&] {
            switch (operation.type) {
                case OperationType::TimeTrimesh:
                    return OscillatorRecipeStage::TimeSourceRendering;
                case OperationType::SpectralTrimesh:
                    return OscillatorRecipeStage::SpectralSourceRendering;
                case OperationType::Fft:
                    return OscillatorRecipeStage::ForwardTransform;
                case OperationType::Ifft:
                    return OscillatorRecipeStage::InverseTransform;
                case OperationType::SpectralLayer:
                case OperationType::Add:
                case OperationType::Multiply:
                    return OscillatorRecipeStage::GraphCombining;
            }
            return OscillatorRecipeStage::GraphCombining;
        }();
        auto* performanceCounts = context == nullptr
                ? nullptr
                : context->performanceCounts;
        AudioPerformanceMetrics::ScopedOscillatorRecipeStage measuredStage(
                performanceCounts,
                performanceStage);
        const int count = valueCount(operation.outputDomain, frameSize);
        auto leftOutput = slot(operation.outputs[0], 0, count);
        auto rightOutput = slot(operation.outputs[0], 1, count);
        switch (operation.type) {
            case OperationType::TimeTrimesh:
            case OperationType::SpectralTrimesh: {
                auto* sourcePerformance = performanceCounts == nullptr
                        ? nullptr
                        : operation.type == OperationType::SpectralTrimesh
                                ? &performanceCounts->spectralSources
                                : &performanceCounts->timeSources;
                operation.source->render(
                        {
                                frameSize,
                                midiNote,
                                activeHarmonicCount,
                                blockSampleOffset,
                                voiceSamplePosition,
                                elapsedSamples,
                                refreshTimeSources,
                                context,
                                &cycleEnvelopes,
                                &frameRandom,
                                &frameCapture,
                                sourcePerformance,
                                phaseHarmonicScale.withSize(
                                        RealFftFullPolarSpectrum::binCountForBufferSize(
                                                frameSize) - 1)
                        },
                        leftOutput,
                        rightOutput);
                break;
            }

            case OperationType::SpectralLayer:
                SpectralFrameGraphCombiner::applyPan(
                        operation.outputDomain,
                        slot(operation.leftInput, 0, count),
                        slot(operation.leftInput, 1, count),
                        leftOutput,
                        rightOutput,
                        operation.pan,
                        operation.multiplicative);
                break;

            case OperationType::Fft: {
                const int binCount = RealFftFullPolarSpectrum::binCountForBufferSize(
                        frameSize);
                for (int channel = 0; channel < 2; ++channel) {
                    auto timeFrame = slot(operation.leftInput, channel, frameSize);
                    auto magnitude = slot(operation.outputs[0], channel, binCount);
                    auto phase = slot(operation.outputs[1], channel, binCount);
                    transformStage.forward(
                            *transform,
                            timeFrame,
                            magnitude,
                            phase,
                            activeHarmonicCount,
                            frameCapture,
                            channel);
                }
                break;
            }
            case OperationType::Ifft: {
                const int binCount = RealFftFullPolarSpectrum::binCountForBufferSize(
                        frameSize);
                for (int channel = 0; channel < 2; ++channel) {
                    auto magnitude = magnitudeScratch.withSize(binCount);
                    auto phase = phaseScratch.withSize(binCount);
                    slot(operation.leftInput, channel, binCount).copyTo(magnitude);
                    if (BinarySignalMath::applyTransfer(
                            magnitude,
                            &operation.leftTransfer,
                            (size_t) channel,
                            activeHarmonicCount)) {
                        frameCapture.capture(
                                CycleDsp::SpectralStage::MagnitudeOperand,
                                channel,
                                magnitude.section(1, activeHarmonicCount));
                    }
                    slot(operation.rightInput, channel, binCount).copyTo(phase);
                    transformStage.inverse(
                            *transform,
                            magnitude,
                            phase,
                            slot(operation.outputs[0], channel, frameSize),
                            activeHarmonicCount,
                            hasSpectralMesh,
                            frameCapture,
                            channel);
                }
                break;
            }

            case OperationType::Add:
                for (int channel = 0; channel < 2; ++channel) {
                    auto output = slot(operation.outputs[0], channel, count);
                    graphCombiner.add(
                            {
                                    operation.leftInput >= 0
                                            ? slot(operation.leftInput, channel, count)
                                            : Buffer<float> {},
                                    &operation.leftTransfer,
                                    operation.leftInput >= 0
                            },
                            {
                                    operation.rightInput >= 0
                                            ? slot(operation.rightInput, channel, count)
                                            : Buffer<float> {},
                                    &operation.rightTransfer,
                                    operation.rightInput >= 0
                            },
                            output,
                            magnitudeScratch.withSize(count),
                            operation.outputDomain,
                            channel);
                }
                break;

            case OperationType::Multiply:
                for (int channel = 0; channel < 2; ++channel) {
                    auto output = slot(operation.outputs[0], channel, count);
                    graphCombiner.multiply(
                            {
                                    slot(operation.leftInput, channel, count),
                                    &operation.leftTransfer,
                                    true
                            },
                            {
                                    slot(operation.rightInput, channel, count),
                                    &operation.rightTransfer,
                                    true
                            },
                            output,
                            magnitudeScratch.withSize(count),
                            operation.outputDomain,
                            channel);
                }
                break;
        }
    }

    slot(outputSlot, 0, frameSize).copyTo(left);
    slot(outputSlot, 1, frameSize).copyTo(right);
    ++renderCount;
    return true;
}

int SpectralOscillatorFrameRenderer::valueCount(
        PortDomain domain,
        int frameSize) {
    return domain == PortDomain::SpectralMagnitudeSignal
                    || domain == PortDomain::SpectralPhaseSignal
            ? RealFftFullPolarSpectrum::binCountForBufferSize(frameSize)
            : frameSize;
}

Buffer<float> SpectralOscillatorFrameRenderer::slot(
        int slotIndex,
        int channel,
        int valueCount) {
    return {
            slotMemory.get() + (2 * slotIndex + channel) * slotStride,
            valueCount
    };
}

void SpectralOscillatorFrameRenderer::prepareFrameRandom(
        const PreparedOscillatorProcessContext* context) {
    const bool hasDeterministicRandomSeed = context != nullptr
            && context->voice != nullptr
            && context->voice->hasDeterministicRandomSeed;
    const bool hasLifecycleSeed = context != nullptr
            && context->voice != nullptr
            && context->voice->hasLifecycleSeed;
    int64_t seed = GuideCurveSnapshotProvider::visualizationSeed(PortDomain::TimeSignal);
    if (hasDeterministicRandomSeed) {
        seed = context->voice->deterministicRandomSeed + 1;
    } else if (hasLifecycleSeed) {
        seed = context->voice->lifecycleSeed;
    }
    if (lifecycleSeedReady && frameRandomSeed == seed) {
        return;
    }

    frameRandomSeed = seed;
    lifecycleSeedReady = true;
    cycleEnvelopes.setVoiceLifecycleSeed(hasDeterministicRandomSeed
            ? context->voice->deterministicRandomSeed
            : seed);
    frameRandom.setSeed(seed);
    uint32_t timeOffsetSeed = (uint32_t) seed;
    uint32_t magnitudeOffsetSeed = (uint32_t) seed;
    uint32_t phaseOffsetSeed = (uint32_t) seed;
    if (hasDeterministicRandomSeed) {
        // Cycle 1 takes only the time-offset draw from the filter voice stream.
        // Keep the other offset translation from advancing retained layer noise.
        Random offsetRandom(seed);
        timeOffsetSeed = (uint32_t) offsetRandom.nextInt();
        magnitudeOffsetSeed = (uint32_t) offsetRandom.nextInt();
        phaseOffsetSeed = (uint32_t) offsetRandom.nextInt();
        frameRandom.nextInt();
    }
    for (auto& operation : operations) {
        if (operation.source != nullptr) {
            operation.source->setVoiceLifecycleSeeds(
                    timeOffsetSeed,
                    magnitudeOffsetSeed,
                    phaseOffsetSeed);
        }
    }
}

}
