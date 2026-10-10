#pragma once

#include <vector>

#include "Graph/GraphCompiler.h"

namespace CycleV2 {

class OscillatorRegionPlanView {
public:
    OscillatorRegionPlanView(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region);

    bool isValid() const { return valid; }
    bool containsStep(int stepIndex) const;
    const GraphStepInput* inputForPort(
            const GraphExecutionStep& step,
            int portIndex) const;
    bool inputComesFromRegion(
            const GraphExecutionStep& step,
            int portIndex) const;

private:
    std::vector<bool> regionSteps;
    bool valid {};
};

}
