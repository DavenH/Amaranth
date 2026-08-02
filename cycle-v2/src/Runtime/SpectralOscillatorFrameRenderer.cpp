#include "SpectralOscillatorFrameRenderer.h"

#include <Audio/CycleDsp/OscillatorLaneRasterizer.h>
#include <Curve/Curve.h>

#include <algorithm>

namespace CycleV2 {

namespace {

bool isPowerOfTwo(int value) {
    return value > 1 && (value & (value - 1)) == 0;
}

bool supportedRole(AudioModuleRole role) {
    return role == AudioModuleRole::MeshSource
            || role == AudioModuleRole::Fft
            || role == AudioModuleRole::Ifft
            || role == AudioModuleRole::Add
            || role == AudioModuleRole::Multiply;
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
        if (step.audioRole == AudioModuleRole::MeshSource) {
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
        int maximumFrameSizeToUse) {
    if (!supports(plan, region) || !isPowerOfTwo(maximumFrameSizeToUse)) {
        return false;
    }

    maximumFrameSize = maximumFrameSizeToUse;
    if (Curve::table == nullptr) {
        Curve::calcTable();
    }
    slotStride = maximumFrameSize + 2;
    outputSlot = -1;
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
                || (step.audioRole != AudioModuleRole::MeshSource
                        && operation.leftInput < 0)
                || ((step.audioRole == AudioModuleRole::Ifft
                            || step.audioRole == AudioModuleRole::Add
                            || step.audioRole == AudioModuleRole::Multiply)
                        && operation.rightInput < 0)) {
            return false;
        }

        switch (step.audioRole) {
            case AudioModuleRole::MeshSource: {
                operation.configuration = std::dynamic_pointer_cast<
                        const TrimeshConfiguration>(step.configuration.value);
                if (operation.configuration == nullptr
                        || operation.configuration->mesh == nullptr) {
                    return false;
                }
                if (operation.outputDomain == PortDomain::TimeSignal) {
                    operation.type = OperationType::TimeTrimesh;
                    operation.timeState = std::make_unique<
                            Rasterization::VoiceCycleState>();
                    operation.timeRasterizer = std::make_unique<
                            Rasterization::VoiceRasterizer>();
                    operation.timeRasterizer->setCalcDepthDimensions(false);
                    operation.timeRasterizer->setScalingMode(
                            Rasterization::PointScalingMode::Bipolar);
                    operation.timeRasterizer->prepare(
                            Rasterization::VoiceRasterizerPreparation::forMesh(
                                    *const_cast<Mesh*>(operation.configuration->mesh.get())),
                            { operation.timeState.get() });
                } else {
                    operation.type = OperationType::SpectralTrimesh;
                    operation.spectralRasterizer = std::make_unique<TrimeshBlockwiseDsp>();
                    operation.spectralRasterizer->prepare(
                            const_cast<Mesh*>(operation.configuration->mesh.get()),
                            operation.configuration->morph,
                            operation.configuration->primaryViewAxis,
                            false);
                }
                break;
            }
            case AudioModuleRole::Fft:      operation.type = OperationType::Fft; break;
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
    slotMemory.resize(slotCount * slotStride);

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
    for (auto& operation : operations) {
        if (operation.timeState != nullptr) {
            operation.timeState->reset();
        }
        if (operation.timeRasterizer != nullptr) {
            operation.timeRasterizer->orphanOldVerts();
        }
    }
}

bool SpectralOscillatorFrameRenderer::renderFrame(
        int frameSize,
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

    for (auto& operation : operations) {
        const int count = valueCount(operation.outputDomain, frameSize);
        auto output = slot(operation.outputs[0], count);
        switch (operation.type) {
            case OperationType::TimeTrimesh:
                CycleDsp::OscillatorLaneRasterizer::renderFixedFrame(
                        *operation.timeRasterizer,
                        {
                                const_cast<Mesh*>(operation.configuration->mesh.get()),
                                operation.configuration->morph,
                                0.f,
                                0
                        },
                        output);
                break;

            case OperationType::SpectralTrimesh:
                operation.spectralRasterizer->renderPreparedInto(output);
                break;

            case OperationType::Fft: {
                const int binCount = RealFftFullPolarSpectrum::binCountForBufferSize(
                        frameSize);
                auto magnitude = slot(operation.outputs[0], binCount);
                auto phase = slot(operation.outputs[1], binCount);
                transform->forward(slot(operation.leftInput, frameSize));
                transform->copyFullPolarSpectrumTo(magnitude, phase);
                break;
            }
            case OperationType::Ifft: {
                const int binCount = RealFftFullPolarSpectrum::binCountForBufferSize(
                        frameSize);
                transform->setFullPolarSpectrum(
                        slot(operation.leftInput, binCount),
                        slot(operation.rightInput, binCount));
                transform->inverse(output);
                break;
            }

            case OperationType::Add:
                slot(operation.leftInput, count).copyTo(output);
                output.add(slot(operation.rightInput, count));
                if (operation.outputDomain == PortDomain::SpectralMagnitudeSignal) {
                    output.threshLT(0.f);
                }
                break;

            case OperationType::Multiply:
                slot(operation.leftInput, count).copyTo(output);
                output.mul(slot(operation.rightInput, count));
                if (operation.outputDomain == PortDomain::SpectralMagnitudeSignal) {
                    output.threshLT(0.f);
                }
                break;
        }
    }

    slot(outputSlot, frameSize).copyTo(left);
    left.copyTo(right);
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
        int valueCount) {
    return { slotMemory.get() + slotIndex * slotStride, valueCount };
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

}
