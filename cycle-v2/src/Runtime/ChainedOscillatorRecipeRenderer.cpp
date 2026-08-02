#include "ChainedOscillatorRecipeRenderer.h"

#include <algorithm>

namespace CycleV2 {

namespace {

bool supportedRole(AudioModuleRole role) {
    return role == AudioModuleRole::MeshSource
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

}

bool ChainedOscillatorRecipeRenderer::supports(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    if (region.strategy != OscillatorExecutionStrategy::ChainedPerLane
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
        const auto* left = inputForPort(step, 0);
        const auto* right = inputForPort(step, 1);
        if (left == nullptr
                || right == nullptr
                || left->sourceStepIndex < 0
                || right->sourceStepIndex < 0
                || left->sourceStepIndex >= (int) regionSteps.size()
                || right->sourceStepIndex >= (int) regionSteps.size()
                || !regionSteps[(size_t) left->sourceStepIndex]
                || !regionSteps[(size_t) right->sourceStepIndex]) {
            return false;
        }
    }
    return true;
}

bool ChainedOscillatorRecipeRenderer::prepare(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        int maximumCycleSamplesToUse) {
    if (!supports(plan, region) || maximumCycleSamplesToUse <= 0) {
        return false;
    }

    maximumCycleSamples = maximumCycleSamplesToUse;
    outputOperation = -1;
    operations.clear();
    operations.reserve(region.stepIndices.size());
    std::vector<int> operationForStep(plan.steps.size(), -1);

    for (const int stepIndex : region.stepIndices) {
        const auto& step = plan.steps[(size_t) stepIndex];
        Operation operation;
        if (step.audioRole == AudioModuleRole::MeshSource) {
            const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
                    step.configuration.value);
            operation.trimesh = std::make_unique<TrimeshOscillatorCycleRenderer>();
            if (!operation.trimesh->prepare(configuration, region.laneCount)) {
                return false;
            }
        } else {
            operation.type = step.audioRole == AudioModuleRole::Add
                    ? OperationType::Add
                    : OperationType::Multiply;
            const auto* left = inputForPort(step, 0);
            const auto* right = inputForPort(step, 1);
            operation.leftInput = operationForStep[(size_t) left->sourceStepIndex];
            operation.rightInput = operationForStep[(size_t) right->sourceStepIndex];
            if (operation.leftInput < 0 || operation.rightInput < 0) {
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
    for (auto& operation : operations) {
        if (operation.trimesh != nullptr) {
            operation.trimesh->reset();
        }
    }
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

    for (int operationIndex = 0; operationIndex < (int) operations.size(); ++operationIndex) {
        auto& operation = operations[(size_t) operationIndex];
        auto outputLeft = operationBuffer(operationIndex, 0, request.sampleCount);
        auto outputRight = operationBuffer(operationIndex, 1, request.sampleCount);
        if (operation.trimesh != nullptr) {
            operation.trimesh->renderCycle(request, outputLeft, outputRight);
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
        if (operation.type == OperationType::Add) {
            outputLeft.add(rightLeft);
            outputRight.add(rightRight);
        } else {
            outputLeft.mul(rightLeft);
            outputRight.mul(rightRight);
        }
    }

    operationBuffer(outputOperation, 0, request.sampleCount).copyTo(left);
    operationBuffer(outputOperation, 1, request.sampleCount).copyTo(right);
}

Buffer<float> ChainedOscillatorRecipeRenderer::operationBuffer(
        int operationIndex,
        int channel,
        int sampleCount) {
    const int offset = (operationIndex * 2 + channel) * maximumCycleSamples;
    return { operationMemory.get() + offset, sampleCount };
}

}
