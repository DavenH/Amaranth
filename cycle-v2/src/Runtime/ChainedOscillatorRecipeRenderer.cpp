#include "Runtime/ChainedOscillatorRecipeRenderer.h"

#include "Runtime/AudioPerformanceMetrics.h"
#include "Runtime/OscillatorRegionPlanView.h"

#include <Util/Arithmetic.h>

#include <algorithm>

namespace CycleV2 {

namespace {

bool supportedRole(AudioModuleRole role) {
    return role == AudioModuleRole::MeshSource
            || role == AudioModuleRole::WaveSource
            || role == AudioModuleRole::SpectralLayer
            || role == AudioModuleRole::Add
            || role == AudioModuleRole::Multiply;
}

}

bool ChainedOscillatorRecipeRenderer::supports(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    const OscillatorRegionPlanView regionView(plan, region);
    if (region.strategy != OscillatorExecutionStrategy::ChainedPerLane
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
        if (step.audioRole == AudioModuleRole::MeshSource
                || step.audioRole == AudioModuleRole::WaveSource) {
            continue;
        }
        if (step.audioRole == AudioModuleRole::SpectralLayer) {
            if (!regionView.inputComesFromRegion(step, 0)) {
                return false;
            }
            continue;
        }
        const bool leftInRegion = regionView.inputComesFromRegion(step, 0);
        const bool rightInRegion = regionView.inputComesFromRegion(step, 1);
        if ((step.audioRole == AudioModuleRole::Add && !leftInRegion && !rightInRegion)
                || (step.audioRole == AudioModuleRole::Multiply
                        && (!leftInRegion || !rightInRegion))) {
            return false;
        }
    }
    return true;
}

bool ChainedOscillatorRecipeRenderer::prepare(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        int maximumCycleSamplesToUse,
        const std::vector<NodeAudioProcessor*>& processors,
        int laneCount,
        const String& pitchEnvelopeNodeId) {
    if (!supports(plan, region) || maximumCycleSamplesToUse <= 0) {
        return false;
    }

    maximumCycleSamples = maximumCycleSamplesToUse;
    if (!cycleEnvelopes.prepare(
            plan,
            region,
            processors,
            laneCount,
            pitchEnvelopeNodeId)) {
        return false;
    }
    outputOperation = -1;
    operations.clear();
    operations.reserve(region.stepIndices.size());
    const OscillatorRegionPlanView regionView(plan, region);
    std::vector<int> operationForStep(plan.steps.size(), -1);

    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        Operation operation;
        if (step.audioRole == AudioModuleRole::MeshSource
                || step.audioRole == AudioModuleRole::WaveSource) {
            if (!PreparedTrimeshMorphBinding::supports(plan, step)) {
                return false;
            }
            const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
                    step.configuration.value);
            operation.configuration = configuration;
            operation.trimesh = std::make_unique<TrimeshOscillatorCycleRenderer>();
            if (!operation.trimesh->prepare(configuration, region.laneCount)) {
                return false;
            }
            operation.gain = configuration->enabled ? configuration->gain : 0.f;
            operation.morphBinding.bind(plan, step);
            operation.morphResolver.reset(configuration->morph, true);
        } else if (step.audioRole == AudioModuleRole::SpectralLayer) {
            const auto configuration = std::dynamic_pointer_cast<
                    const PanConfiguration>(step.configuration.value);
            const auto* input = regionView.inputForPort(step, 0);
            if (configuration == nullptr || input == nullptr) {
                return false;
            }
            operation.type = OperationType::Pan;
            operation.leftInput = operationForStep[(size_t) input->sourceStepIndex];
            Arithmetic::getPans(
                    configuration->pan,
                    operation.leftPan,
                    operation.rightPan);
            if (operation.leftInput < 0) {
                return false;
            }
        } else {
            operation.type = step.audioRole == AudioModuleRole::Add
                    ? OperationType::Add
                    : OperationType::Multiply;
            const auto* left = regionView.inputForPort(step, 0);
            const auto* right = regionView.inputForPort(step, 1);
            const auto operationIndexForInput = [&](const GraphStepInput* input) {
                return input != nullptr
                                && input->sourceStepIndex >= 0
                                && input->sourceStepIndex < (int) operationForStep.size()
                        ? operationForStep[(size_t) input->sourceStepIndex]
                        : -1;
            };
            operation.leftInput = operationIndexForInput(left);
            operation.rightInput = operationIndexForInput(right);
            if ((operation.type == OperationType::Add
                        && operation.leftInput < 0
                        && operation.rightInput < 0)
                    || (operation.type == OperationType::Multiply
                        && (operation.leftInput < 0 || operation.rightInput < 0))) {
                return false;
            }
        }
        operationForStep[(size_t) stepIndex] = (int) operations.size();
        operations.push_back(std::move(operation));
    }

    outputOperation = operationForStep[(size_t) region.materializationStepIndex];
    if (outputOperation < 0) {
        return false;
    }
    operationMemory.resize(
            2 * maximumCycleSamples * (int) operations.size());
    reset();
    return true;
}

void ChainedOscillatorRecipeRenderer::reset() {
    lifecycleSeedReady = false;
    cycleEnvelopes.reset();
    for (auto& operation : operations) {
        if (operation.trimesh != nullptr) {
            operation.trimesh->reset();
        }
        if (operation.configuration != nullptr) {
            operation.morphResolver.reset(operation.configuration->morph, true);
        }
        operation.lastMorphFrontier = 0;
    }
}

void ChainedOscillatorRecipeRenderer::applyLifecycleEvent(
        const NoteLifecycleEvent& event) {
    if (event.type == NoteLifecycleType::NoteOn) {
        lifecycleSeedReady = false;
    }
    cycleEnvelopes.applyLifecycleEvent(event);
}

void ChainedOscillatorRecipeRenderer::advanceCycleEnvelopes(
        int laneIndex,
        int sampleCount,
        double normalizedTimeIncrement,
        const PreparedOscillatorProcessContext* context) {
    prepareFrameRandom(context);
    cycleEnvelopes.advanceLane(
            laneIndex,
            sampleCount,
            normalizedTimeIncrement);
}

void ChainedOscillatorRecipeRenderer::renderCycle(
        const ChainedCycleRenderRequest& request,
        Buffer<float> left,
        Buffer<float> right) {
    if (request.sampleCount <= 0
            || request.sampleCount > maximumCycleSamples
            || left.size() != request.sampleCount
            || right.size() != request.sampleCount
            || outputOperation < 0) {
        left.zero();
        right.zero();
        return;
    }

    prepareFrameRandom(request.processContext);
    for (int operationIndex = 0; operationIndex < (int) operations.size(); ++operationIndex) {
        auto& operation = operations[(size_t) operationIndex];
        auto outputLeft = operationBuffer(operationIndex, 0, request.sampleCount);
        auto outputRight = operationBuffer(operationIndex, 1, request.sampleCount);
        auto* performanceCounts = request.processContext == nullptr
                ? nullptr
                : request.processContext->performanceCounts;
        const OscillatorRecipeStage performanceStage = operation.trimesh != nullptr
                ? OscillatorRecipeStage::TimeSourceRendering
                : OscillatorRecipeStage::GraphCombining;
        AudioPerformanceMetrics::ScopedOscillatorRecipeStage measuredStage(
                performanceCounts,
                performanceStage);
        if (operation.trimesh != nullptr) {
            MorphPosition morph = operation.configuration->morph;
            auto* sourcePerformance = performanceCounts == nullptr ? nullptr : &performanceCounts->timeSources;
            CycleDsp::ScopedSourceRenderStage morphStage(
                    sourcePerformance, CycleDsp::SourceRenderStage::MorphResolution);
            if (request.processContext != nullptr) {
                const uint64_t frontier = (uint64_t) request.cycleStartSample;
                const size_t elapsedSamples = frontier > operation.lastMorphFrontier
                        ? (size_t) (frontier - operation.lastMorphFrontier)
                        : 0;
                morph = operation.morphResolver.resolve(
                        operation.morphBinding.inputsFor(
                                *request.processContext,
                                request.blockSampleOffset,
                                request.cycleStartSample,
                                &cycleEnvelopes,
                                request.laneIndex),
                        operation.configuration->morph,
                        PortDomain::TimeSignal,
                        operation.configuration->primaryViewAxis,
                        request.blockSampleOffset,
                        elapsedSamples,
                        request.processContext->timing.sampleRate,
                        operation.configuration->scratchSourceEnabled);
                operation.lastMorphFrontier = std::max(
                        operation.lastMorphFrontier,
                        frontier);
            }
            morphStage.finish();
            operation.trimesh->renderCycleAtMorph(
                    request,
                    morph,
                    frameRandom.nextInt(GuideCurveProvider::tableSize),
                    outputLeft,
                    outputRight);
            if (operation.gain != 1.f) {
                CycleDsp::ScopedSourceRenderStage gainStage(
                        sourcePerformance, CycleDsp::SourceRenderStage::Gain);
                outputLeft.mul(operation.gain);
                outputRight.mul(operation.gain);
            }
            continue;
        }

        if (operation.type == OperationType::Pan) {
            operationBuffer(operation.leftInput, 0, request.sampleCount).copyTo(outputLeft);
            operationBuffer(operation.leftInput, 1, request.sampleCount).copyTo(outputRight);
            outputLeft.mul(operation.leftPan);
            outputRight.mul(operation.rightPan);
            continue;
        }
        if (operation.type == OperationType::Add) {
            outputLeft.zero();
            outputRight.zero();
            if (operation.leftInput >= 0) {
                outputLeft.add(operationBuffer(
                        operation.leftInput, 0, request.sampleCount));
                outputRight.add(operationBuffer(
                        operation.leftInput, 1, request.sampleCount));
            }
            if (operation.rightInput >= 0) {
                outputLeft.add(operationBuffer(
                        operation.rightInput, 0, request.sampleCount));
                outputRight.add(operationBuffer(
                        operation.rightInput, 1, request.sampleCount));
            }
            continue;
        }

        operationBuffer(operation.leftInput, 0, request.sampleCount).copyTo(outputLeft);
        operationBuffer(operation.leftInput, 1, request.sampleCount).copyTo(outputRight);
        const auto rightLeft = operationBuffer(
                operation.rightInput,
                0,
                request.sampleCount);
        const auto rightRight = operationBuffer(
                operation.rightInput,
                1,
                request.sampleCount);
        outputLeft.mul(rightLeft);
        outputRight.mul(rightRight);
    }

    operationBuffer(outputOperation, 0, request.sampleCount).copyTo(left);
    operationBuffer(outputOperation, 1, request.sampleCount).copyTo(right);
}

void ChainedOscillatorRecipeRenderer::prepareFrameRandom(
        const PreparedOscillatorProcessContext* context) {
    const bool hasDeterministicRandomSeed = context != nullptr
            && context->voice != nullptr
            && context->voice->hasDeterministicRandomSeed;
    const bool hasLifecycleSeed = context != nullptr
            && context->voice != nullptr
            && context->voice->hasLifecycleSeed;
    int64_t seed = GuideCurveSnapshotProvider::visualizationSeed(PortDomain::TimeSignal);
    if (hasDeterministicRandomSeed) {
        seed = context->voice->deterministicRandomSeed + 2;
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
    const uint32_t offsetSeed = hasDeterministicRandomSeed
            ? (uint32_t) frameRandom.nextInt()
            : (uint32_t) seed;
    for (auto& operation : operations) {
        if (operation.trimesh != nullptr) {
            operation.trimesh->setVoiceLifecycleSeed(offsetSeed);
        }
    }
}

Buffer<float> ChainedOscillatorRecipeRenderer::operationBuffer(
        int operationIndex,
        int channel,
        int sampleCount) {
    const int offset = (operationIndex * 2 + channel) * maximumCycleSamples;
    return { operationMemory.get() + offset, sampleCount };
}

}
