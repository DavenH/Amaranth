#pragma once

#include "Runtime/ChainedOscillatorRegionRuntime.h"
#include "Graph/GraphCompiler.h"
#include "Nodes/Trimesh/Dsp/TrimeshOscillatorCycleRenderer.h"

#include <Array/ScopedAlloc.h>

#include <memory>
#include <vector>

namespace CycleV2 {

class ChainedOscillatorRecipeRenderer final : public OscillatorCycleRenderer {
public:
    static bool supports(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region);

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            int maximumCycleSamples);
    void reset() override;
    void renderCycle(
            const ChainedCycleRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override;

private:
    enum class OperationType {
        Trimesh,
        Add,
        Multiply
    };

    struct Operation {
        OperationType type { OperationType::Trimesh };
        int leftInput { -1 };
        int rightInput { -1 };
        float gain { 1.f };
        std::unique_ptr<TrimeshOscillatorCycleRenderer> trimesh;
    };

    Buffer<float> operationBuffer(
            int operationIndex,
            int channel,
            int sampleCount);

    int maximumCycleSamples {};
    int outputOperation { -1 };
    std::vector<Operation> operations;
    ScopedAlloc<float> operationMemory;
};

}
