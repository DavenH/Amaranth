#pragma once

#include <cstdint>
#include <vector>

#include "Graph/GraphCompiler.h"

namespace CycleV2 {

std::vector<uint8_t> probeExecutionSteps(
        const GraphExecutionPlan& plan,
        int sourceStepIndex);
std::vector<uint8_t> probeExecutionSteps(
        const GraphExecutionPlan& plan,
        const std::vector<int>& sourceStepIndices);

}
