#pragma once

#include "Graph/GraphCompiler.h"
#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/TrimeshMorphResolver.h"

namespace CycleV2 {

class PreparedTrimeshMorphBinding {
public:
    static bool supports(
            const GraphExecutionPlan& plan,
            const GraphExecutionStep& step);

    void bind(const GraphExecutionStep& step);
    TrimeshMorphInputs inputsFor(
            const PreparedOscillatorProcessContext& context) const;

private:
    std::array<int, 3> morphInputBuffers { -1, -1, -1 };
    int scratchBuffer { -1 };
};

}
