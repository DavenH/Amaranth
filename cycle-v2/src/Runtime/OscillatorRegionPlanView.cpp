#include "Runtime/OscillatorRegionPlanView.h"

#include <algorithm>

namespace CycleV2 {

OscillatorRegionPlanView::OscillatorRegionPlanView(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) :
        regionSteps(plan.steps.size()) {
    if (region.stepIndices.empty() || region.materializationStepIndex < 0) {
        return;
    }
    for (const int stepIndex : region.stepIndices) {
        if (stepIndex < 0 || stepIndex >= (int) regionSteps.size()) {
            return;
        }
        regionSteps[(size_t) stepIndex] = true;
    }
    valid = containsStep(region.materializationStepIndex);
}

bool OscillatorRegionPlanView::containsStep(int stepIndex) const {
    return stepIndex >= 0
            && stepIndex < (int) regionSteps.size()
            && regionSteps[(size_t) stepIndex];
}

const GraphStepInput* OscillatorRegionPlanView::inputForPort(
        const GraphExecutionStep& step,
        int portIndex) const {
    const auto found = std::find_if(
            step.inputs.begin(),
            step.inputs.end(),
            [&](const GraphStepInput& input) {
                return input.destPortIndex == portIndex;
            });
    return found != step.inputs.end() ? &*found : nullptr;
}

bool OscillatorRegionPlanView::inputComesFromRegion(
        const GraphExecutionStep& step,
        int portIndex) const {
    const GraphStepInput* input = inputForPort(step, portIndex);
    return input != nullptr && containsStep(input->sourceStepIndex);
}

}
