#pragma once

#include "Runtime/ChainedOscillatorRegionRuntime.h"
#include "Runtime/PreparedCycleEnvelopeBank.h"
#include "Runtime/PreparedTrimeshMorphBinding.h"
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
            int maximumCycleSamples,
            const std::vector<NodeAudioProcessor*>& processors = {},
            int laneCount = 1);
    void reset() override;
    void applyLifecycleEvent(const NoteLifecycleEvent& event) override;
    void renderCycle(
            const ChainedCycleRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) override;

private:
    enum class OperationType {
        Trimesh,
        Pan,
        Add,
        Multiply
    };

    struct Operation {
        OperationType type { OperationType::Trimesh };
        int leftInput { -1 };
        int rightInput { -1 };
        float gain { 1.f };
        float leftPan { 1.f };
        float rightPan { 1.f };
        std::shared_ptr<const TrimeshConfiguration> configuration;
        std::unique_ptr<TrimeshOscillatorCycleRenderer> trimesh;
        PreparedTrimeshMorphBinding morphBinding;
        TrimeshMorphResolver morphResolver;
        uint64_t lastMorphFrontier {};
    };

    Buffer<float> operationBuffer(
            int operationIndex,
            int channel,
            int sampleCount);
    void prepareFrameRandom(const PreparedOscillatorProcessContext* context);

    int maximumCycleSamples {};
    int outputOperation { -1 };
    std::vector<Operation> operations;
    PreparedCycleEnvelopeBank cycleEnvelopes;
    ScopedAlloc<float> operationMemory;
    Random frameRandom;
    uint32_t lifecycleSeed {};
    bool lifecycleSeedReady {};
};

}
