#include "Runtime/SpectralOscillatorFrameRenderer.h"

#include "Graph/NodeParameterMap.h"

#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>
#include <Curve/Curve.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

#include <algorithm>

namespace CycleV2 {

namespace {

bool isPowerOfTwo(int value) {
    return value > 1 && (value & (value - 1)) == 0;
}

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

void captureStage(
        const PreparedOscillatorProcessContext* context,
        CycleDsp::SpectralStage stage,
        size_t frameIndex,
        uint64_t voiceSampleFrontier,
        int midiNote,
        int channel,
        Buffer<float> primary,
        Buffer<float> secondary = {}) {
    if (context == nullptr
            || context->voice == nullptr
            || context->voice->spectralStageCapture == nullptr) {
        return;
    }

    context->voice->spectralStageCapture->capture({
            stage,
            frameIndex,
            voiceSampleFrontier,
            midiNote,
            channel,
            primary,
            secondary
    });
}

const GraphStepInput* inputForPort(
        const GraphExecutionStep& step,
        int portIndex) {
    const auto found = std::find_if(
            step.inputs.begin(),
            step.inputs.end(),
            [&](const GraphStepInput& input) {
                return input.destPortIndex == portIndex;
            });
    return found != step.inputs.end() ? &*found : nullptr;
}

bool inputComesFromRegion(
        const GraphStepInput* input,
        const std::vector<bool>& regionSteps) {
    return input != nullptr
            && input->sourceStepIndex >= 0
            && input->sourceStepIndex < (int) regionSteps.size()
            && regionSteps[(size_t) input->sourceStepIndex];
}

void applyPan(
        PortDomain domain,
        Buffer<float> source,
        Buffer<float> secondarySource,
        Buffer<float> left,
        Buffer<float> right,
        float pan,
        bool multiplicative) {
    float leftPan {};
    float rightPan {};
    Arithmetic::getPans(pan, leftPan, rightPan);
    source.copyTo(left);
    secondarySource.copyTo(right);
    if (domain == PortDomain::SpectralMagnitudeSignal && multiplicative) {
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(left, leftPan);
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(right, rightPan);
    } else {
        left.mul(leftPan);
        right.mul(rightPan);
    }
}

}

bool SpectralOscillatorFrameRenderer::supports(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    if (region.strategy != OscillatorExecutionStrategy::SharedSpectralFrame
            || region.stepIndices.empty()
            || region.materializationStepIndex < 0) {
        return false;
    }

    std::vector<bool> regionSteps(plan.steps.size());
    for (const int stepIndex : region.stepIndices) {
        if (stepIndex < 0
                || stepIndex >= (int) plan.steps.size()
                || !supportedRole(plan.steps[(size_t) stepIndex].audioRole)) {
            return false;
        }
        regionSteps[(size_t) stepIndex] = true;
    }
    if (region.materializationStepIndex >= (int) regionSteps.size()
            || !regionSteps[(size_t) region.materializationStepIndex]) {
        return false;
    }

    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        if (sourceRole(step.audioRole)) {
            if (!PreparedTrimeshMorphBinding::supports(plan, step)) {
                return false;
            }
            continue;
        }
        if (!inputComesFromRegion(inputForPort(step, 0), regionSteps)) {
            return false;
        }
        if ((step.audioRole == AudioModuleRole::Ifft
                    || step.audioRole == AudioModuleRole::Add
                    || step.audioRole == AudioModuleRole::Multiply)
                && !inputComesFromRegion(inputForPort(step, 1), regionSteps)) {
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
        int laneCount) {
    if (!supports(plan, region) || !isPowerOfTwo(maximumFrameSizeToUse)) {
        return false;
    }

    maximumFrameSize = maximumFrameSizeToUse;
    if (!cycleEnvelopes.prepare(plan, region, processors, laneCount)) {
        return false;
    }
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
    slotStride = maximumFrameSize + 2;
    outputSlot = -1;
    hasSpectralMesh = false;
    operations.clear();
    operations.reserve(region.stepIndices.size());
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

        const auto inputSlot = [&](int portIndex) {
            const auto* input = inputForPort(step, portIndex);
            if (input == nullptr
                    || input->sourceStepIndex < 0
                    || input->sourceOutputIndex < 0
                    || input->sourceOutputIndex >= 2) {
                return -1;
            }
            return slotsForStep[(size_t) input->sourceStepIndex]
                    [(size_t) input->sourceOutputIndex];
        };
        operation.leftInput = inputSlot(0);
        operation.rightInput = inputSlot(1);

        if (operation.outputs[0] < 0
                || (step.audioRole == AudioModuleRole::Fft
                        && operation.outputs[1] < 0)
                || (!sourceRole(step.audioRole)
                        && operation.leftInput < 0)
                || ((step.audioRole == AudioModuleRole::Ifft
                            || step.audioRole == AudioModuleRole::Add
                            || step.audioRole == AudioModuleRole::Multiply)
                        && operation.rightInput < 0)) {
            return false;
        }

        switch (step.audioRole) {
            case AudioModuleRole::MeshSource:
            case AudioModuleRole::WaveSource: {
                operation.configuration = std::dynamic_pointer_cast<
                        const TrimeshConfiguration>(step.configuration.value);
                if (operation.configuration == nullptr
                        || operation.configuration->mesh == nullptr) {
                    return false;
                }
                operation.morphBinding.bind(plan, step);
                operation.morphResolver.reset(operation.configuration->morph, true);
                if (operation.outputDomain == PortDomain::TimeSignal) {
                    operation.type = OperationType::TimeTrimesh;
                    operation.timeState = std::make_unique<
                            Rasterization::VoiceCycleState>();
                    operation.timeRasterizer = std::make_unique<
                            Rasterization::VoiceRasterizer>();
                    operation.timeRasterizer->setCalcDepthDimensions(false);
                    operation.timeRasterizer->setGuideCurveProvider(
                            operation.configuration->guideCurveProvider.get());
                    operation.timeRasterizer->setScalingMode(
                            Rasterization::PointScalingMode::Bipolar);
                    operation.timeRasterizer->prepare(
                            Rasterization::VoiceRasterizerPreparation::forMesh(
                                    *const_cast<Mesh*>(operation.configuration->mesh.get())),
                            { operation.timeState.get() });
                } else {
                    operation.type = OperationType::SpectralTrimesh;
                    auto* spectralMesh = const_cast<Mesh*>(
                            operation.configuration->mesh.get());
                    const bool activeSpectralMesh = operation.configuration->enabled
                            && spectralMesh->hasEnoughCubesForCrossSection();
                    hasSpectralMesh |= activeSpectralMesh;
                    operation.spectralRasterizer = std::make_unique<TrimeshBlockwiseDsp>();
                    operation.spectralRasterizer->setGuideCurveProvider(
                            operation.configuration->guideCurveProvider.get());
                    operation.spectralRasterizer->prepare(
                            spectralMesh,
                            operation.configuration->morph,
                            operation.configuration->primaryViewAxis,
                            false,
                            operation.outputDomain);
                    operation.spectralRasterizer->prepareSampling(
                            (size_t) maximumFrameSize);
                }
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
    magnitudeScratch.resize(maximumBinCount);
    phaseScratch.resize(maximumBinCount);

    transforms.clear();
    for (int frameSize = 2; frameSize <= maximumFrameSize; frameSize *= 2) {
        auto transform = std::make_unique<Transform>();
        transform->allocate(frameSize, Transform::DivFwdByN, true);
        transform->setExclusiveRealtimeAccess(true);
        transforms.push_back(std::move(transform));
    }
    reset();
    return true;
}

void SpectralOscillatorFrameRenderer::reset() {
    renderCount = 0;
    lifecycleSeedReady = false;
    cycleEnvelopes.reset();
    for (auto& operation : operations) {
        if (operation.timeState != nullptr) {
            operation.timeState->reset();
        }
        if (operation.timeRasterizer != nullptr) {
            operation.timeRasterizer->orphanOldVerts();
        }
        if (operation.configuration != nullptr) {
            operation.morphResolver.reset(operation.configuration->morph, true);
        }
    }
}

void SpectralOscillatorFrameRenderer::applyLifecycleEvent(
        const NoteLifecycleEvent& event) {
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
            right);
}

bool SpectralOscillatorFrameRenderer::renderFrame(
        int frameSize,
        int midiNote,
        const PreparedOscillatorProcessContext& context,
        size_t blockSampleOffset,
        double voiceSamplePosition,
        size_t elapsedSamples,
        Buffer<float> left,
        Buffer<float> right) {
    return renderFrameInternal(
            frameSize,
            midiNote,
            &context,
            blockSampleOffset,
            voiceSamplePosition,
            elapsedSamples,
            left,
            right);
}

bool SpectralOscillatorFrameRenderer::renderFrameInternal(
        int frameSize,
        int midiNote,
        const PreparedOscillatorProcessContext* context,
        size_t blockSampleOffset,
        double voiceSamplePosition,
        size_t elapsedSamples,
        Buffer<float> left,
        Buffer<float> right) {
    Transform* transform = transformFor(frameSize);
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
    const uint64_t voiceSampleFrontier = (uint64_t) voiceSamplePosition;
    for (auto& operation : operations) {
        const int count = valueCount(operation.outputDomain, frameSize);
        auto leftOutput = slot(operation.outputs[0], 0, count);
        auto rightOutput = slot(operation.outputs[0], 1, count);
        MorphPosition morph;
        if (operation.configuration != nullptr) {
            const TrimeshMorphInputs inputs = context != nullptr
                    ? operation.morphBinding.inputsFor(
                            *context,
                            blockSampleOffset,
                            voiceSamplePosition,
                            &cycleEnvelopes)
                    : TrimeshMorphInputs {};
            morph = operation.morphResolver.resolve(
                    inputs,
                    operation.configuration->morph,
                    operation.outputDomain,
                    operation.configuration->primaryViewAxis,
                    blockSampleOffset,
                    elapsedSamples,
                    context != nullptr ? context->timing.sampleRate : 44100.0,
                    operation.configuration->scratchSourceEnabled);
        }
        switch (operation.type) {
            case OperationType::TimeTrimesh:
                if (!operation.configuration->enabled) {
                    leftOutput.zero();
                    rightOutput.zero();
                    break;
                }
                CycleDsp::OscillatorLaneRasterizer::renderFixedFrame(
                        *operation.timeRasterizer,
                        {
                                const_cast<Mesh*>(operation.configuration->mesh.get()),
                                morph,
                                0.f,
                                frameRandom.nextInt(
                                        GuideCurveProvider::tableSize)
                        },
                        leftOutput);
                {
                    std::array<float, 3> morphValues {
                            morph.time.getCurrentValue(),
                            morph.red.getCurrentValue(),
                            morph.blue.getCurrentValue()
                    };
                    for (int channel = 0; channel < 2; ++channel) {
                        captureStage(
                                context,
                                CycleDsp::SpectralStage::TimeRaster,
                                renderCount,
                                voiceSampleFrontier,
                                midiNote,
                                channel,
                                leftOutput,
                                { morphValues.data(), (int) morphValues.size() });
                    }
                }
                leftOutput.mul(operation.configuration->gain);
                leftOutput.copyTo(rightOutput);
                break;

            case OperationType::SpectralTrimesh:
                if (!operation.configuration->enabled) {
                    const float identity = operation.outputDomain
                                            == PortDomain::SpectralMagnitudeSignal
                                    && operation.configuration->multiplicative
                            ? 1.f
                            : 0.f;
                    leftOutput.set(identity);
                    rightOutput.set(identity);
                    break;
                }
                operation.spectralRasterizer->setFrequencyMidiNote(
                        midiNote + LogRegionMapping::legacyMidiNoteBias);
                operation.spectralRasterizer->setMorphPosition(morph);
                operation.spectralRasterizer->rasterizePrepared(
                        frameRandom.nextInt(GuideCurveProvider::tableSize));
                leftOutput.zero();
                operation.spectralRasterizer->renderPreparedHarmonicsInto(
                        leftOutput.section(1, count - 1));
                if (operation.outputDomain
                        == PortDomain::SpectralMagnitudeSignal) {
                    const int activeBinCount = jmin(
                            count - 1,
                            LogRegionMapping(
                                    midiNote + LogRegionMapping::legacyMidiNoteBias)
                                    .regionSize());
                    std::array<float, 3> morphValues {
                            morph.time.getCurrentValue(),
                            morph.red.getCurrentValue(),
                            morph.blue.getCurrentValue()
                    };
                    for (int channel = 0; channel < 2; ++channel) {
                        captureStage(
                                context,
                                CycleDsp::SpectralStage::MagnitudeRaster,
                                renderCount,
                                voiceSampleFrontier,
                                midiNote,
                                channel,
                                leftOutput.section(1, activeBinCount),
                                { morphValues.data(), (int) morphValues.size() });
                    }
                }
                leftOutput.mul(operation.configuration->gain);
                if (operation.configuration->appliesSpectralRange
                        && operation.outputDomain == PortDomain::SpectralMagnitudeSignal) {
                    CycleDsp::SpectralLayerCore::shapeMagnitude(
                            leftOutput,
                            operation.configuration->range,
                            !operation.configuration->multiplicative,
                            count);
                } else if (operation.configuration->appliesSpectralRange
                        && operation.outputDomain == PortDomain::SpectralPhaseSignal) {
                    leftOutput.mul(CycleDsp::SpectralLayerCore::phaseOffsetScale(
                            operation.configuration->range) * MathConstants<float>::twoPi);
                }
                if (operation.outputDomain
                        == PortDomain::SpectralMagnitudeSignal) {
                    const int activeBinCount = jmin(
                            count - 1,
                            LogRegionMapping(
                                    midiNote + LogRegionMapping::legacyMidiNoteBias)
                                    .regionSize());
                    for (int channel = 0; channel < 2; ++channel) {
                        captureStage(
                                context,
                                CycleDsp::SpectralStage::MagnitudeOperand,
                                renderCount,
                                voiceSampleFrontier,
                                midiNote,
                                channel,
                                leftOutput.section(1, activeBinCount));
                    }
                }
                leftOutput.copyTo(rightOutput);
                break;

            case OperationType::SpectralLayer:
                applyPan(
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
                const int activeBinCount = jmin(
                        binCount - 1,
                        LogRegionMapping(
                                midiNote + LogRegionMapping::legacyMidiNoteBias)
                                .regionSize());
                for (int channel = 0; channel < 2; ++channel) {
                    auto timeFrame = slot(operation.leftInput, channel, frameSize);
                    captureStage(
                            context,
                            CycleDsp::SpectralStage::TimeFrame,
                            renderCount,
                            voiceSampleFrontier,
                            midiNote,
                            channel,
                            timeFrame);
                    auto magnitude = slot(operation.outputs[0], channel, binCount);
                    auto phase = slot(operation.outputs[1], channel, binCount);
                    transform->forward(timeFrame);
                    transform->copyFullPolarSpectrumTo(magnitude, phase);
                    captureStage(
                            context,
                            CycleDsp::SpectralStage::ForwardFft,
                            renderCount,
                            voiceSampleFrontier,
                            midiNote,
                            channel,
                            magnitude.section(1, activeBinCount),
                            phase.section(1, activeBinCount));
                }
                break;
            }
            case OperationType::Ifft: {
                const int binCount = RealFftFullPolarSpectrum::binCountForBufferSize(
                        frameSize);
                const LogRegionMapping harmonicRegion(
                        midiNote + LogRegionMapping::legacyMidiNoteBias);
                const int activeBinCount = harmonicRegion.regionSize();
                for (int channel = 0; channel < 2; ++channel) {
                    auto magnitude = magnitudeScratch.withSize(binCount);
                    auto phase = phaseScratch.withSize(binCount);
                    slot(operation.leftInput, channel, binCount).copyTo(magnitude);
                    slot(operation.rightInput, channel, binCount).copyTo(phase);
                    captureStage(
                            context,
                            CycleDsp::SpectralStage::PostLayerSpectrum,
                            renderCount,
                            voiceSampleFrontier,
                            midiNote,
                            channel,
                            magnitude.section(1, activeBinCount),
                            phase.section(1, activeBinCount));
                    if (hasSpectralMesh) {
                        const int activeFullPolarBinCount = activeBinCount + 1;
                        CycleDsp::SpectralLayerCore::clearBinsAbove(
                                magnitude,
                                phase,
                                activeFullPolarBinCount);
                    }
                    transform->setFullPolarSpectrum(
                            magnitude,
                            phase);
                    transform->inverse(slot(operation.outputs[0], channel, frameSize));
                    captureStage(
                            context,
                            CycleDsp::SpectralStage::ReconstructedFrame,
                            renderCount,
                            voiceSampleFrontier,
                            midiNote,
                            channel,
                            slot(operation.outputs[0], channel, frameSize));
                }
                break;
            }

            case OperationType::Add:
                for (int channel = 0; channel < 2; ++channel) {
                    auto output = slot(operation.outputs[0], channel, count);
                    slot(operation.leftInput, channel, count).copyTo(output);
                    output.add(slot(operation.rightInput, channel, count));
                    if (operation.outputDomain == PortDomain::SpectralMagnitudeSignal) {
                        output.threshLT(0.f);
                    }
                }
                break;

            case OperationType::Multiply:
                for (int channel = 0; channel < 2; ++channel) {
                    auto output = slot(operation.outputs[0], channel, count);
                    slot(operation.leftInput, channel, count).copyTo(output);
                    output.mul(slot(operation.rightInput, channel, count));
                    if (operation.outputDomain == PortDomain::SpectralMagnitudeSignal) {
                        output.threshLT(0.f);
                    }
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

Transform* SpectralOscillatorFrameRenderer::transformFor(int frameSize) {
    if (!isPowerOfTwo(frameSize) || frameSize > maximumFrameSize) {
        return nullptr;
    }
    int index = 0;
    for (int size = 2; size < frameSize; size *= 2) {
        ++index;
    }
    return index < (int) transforms.size() ? transforms[(size_t) index].get() : nullptr;
}

void SpectralOscillatorFrameRenderer::prepareFrameRandom(
        const PreparedOscillatorProcessContext* context) {
    const bool hasLifecycleSeed = context != nullptr
            && context->voice != nullptr
            && context->voice->hasLifecycleSeed;
    const uint32_t seed = hasLifecycleSeed
            ? context->voice->lifecycleSeed
            : GuideCurveSnapshotProvider::visualizationSeed(PortDomain::TimeSignal);
    if (lifecycleSeedReady && lifecycleSeed == seed) {
        return;
    }

    lifecycleSeed = seed;
    lifecycleSeedReady = true;
    frameRandom.setSeed((int64) seed);
    for (auto& operation : operations) {
        if (operation.timeRasterizer != nullptr) {
            operation.timeRasterizer->updateOffsetSeeds(
                    operation.configuration != nullptr
                            ? (int) operation.configuration->guideAssignmentCount
                            : 0,
                    GuideCurveProvider::tableSize,
                    Rasterization::GuideCurveSeed::voiceLifecycle(seed));
        }
        if (operation.spectralRasterizer != nullptr) {
            operation.spectralRasterizer->setVoiceLifecycleSeed(seed);
        }
    }
}

}
