#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/ChainedOscillatorRecipeRenderer.h"
#include "Runtime/ChainedOscillatorRegionRuntime.h"
#include "Runtime/OscillatorRegionTraversalRenderer.h"
#include "Runtime/SpectralOscillatorFrameRenderer.h"
#include "Runtime/SpectralOscillatorRegionRuntime.h"
#include "Graph/GraphCompiler.h"

#include <Util/Arithmetic.h>

namespace CycleV2 {

const SignalPayload* PreparedOscillatorProcessContext::signalAt(
        int bufferIndex) const {
    return bufferIndex >= 0 && (size_t) bufferIndex < signalBufferCount
            ? signalBuffers + bufferIndex
            : nullptr;
}

namespace {

class PreparedChainedOscillatorRegion final : public PreparedOscillatorRegion {
public:
    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const CompiledVoiceContext& context,
            const AudioExecutionSpec& spec,
            int maximumCycleSamples,
            const std::vector<NodeAudioProcessor*>& processors) {
        traversalRenderer.prepare(context, spec.maximumFrameCount);
        auto preparedRenderer = std::make_unique<ChainedOscillatorRecipeRenderer>();
        if (!preparedRenderer->prepare(
                    plan,
                    region,
                    maximumCycleSamples,
                    processors,
                    context.lanes.order,
                    context.pitchEnvelopeNodeId)
                || !runtime.prepare(
                        spec.maximumFrameCount,
                        maximumCycleSamples,
                        spec.sampleRate,
                        context.lanes)) {
            return false;
        }
        renderer = std::move(preparedRenderer);
        return true;
    }

    void reset() override {
        runtime.reset();
        renderer->reset();
    }

    void applyLifecycleEvent(const NoteLifecycleEvent& event) override {
        renderer->applyLifecycleEvent(event);
    }

    bool process(const PreparedOscillatorProcessContext& context) override {
        return runtime.process(context, *renderer);
    }

    bool renderTraversal(SignalTraversalGrid& grid, int midiNote) override {
        return traversalRenderer.render(grid, midiNote);
    }

private:
    ChainedOscillatorRegionRuntime runtime;
    OscillatorRegionTraversalRenderer traversalRenderer;
    std::unique_ptr<OscillatorCycleRenderer> renderer;
};

class PreparedSpectralOscillatorRegion final : public PreparedOscillatorRegion {
public:
    size_t frameRenderCount() const override { return renderer.frameRenderCount(); }

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const CompiledVoiceContext& context,
            const AudioExecutionSpec& spec,
            int maximumCycleSamples,
            const std::vector<NodeAudioProcessor*>& processors) {
        const int maximumFixedFrameSize = Arithmetic::getNextPow2(
                (float) maximumCycleSamples);
        traversalRenderer.prepare(context, spec.maximumFrameCount);
        return renderer.prepare(
                    plan,
                    region,
                    maximumFixedFrameSize,
                    processors,
                    context.lanes.order,
                    context.pitchEnvelopeNodeId)
                && runtime.prepare(
                        spec.maximumFrameCount,
                        maximumCycleSamples,
                        maximumFixedFrameSize,
                        spec.sampleRate,
                        context.lanes,
                        context.controlIntervalSamples,
                        context.pitchIndependentSpectralControl);
    }

    void reset() override {
        runtime.reset();
        renderer.reset();
    }

    void applyLifecycleEvent(const NoteLifecycleEvent& event) override {
        renderer.applyLifecycleEvent(event);
    }

    bool process(const PreparedOscillatorProcessContext& context) override {
        return runtime.process(context, renderer);
    }

    bool renderTraversal(SignalTraversalGrid& grid, int midiNote) override {
        return traversalRenderer.render(grid, midiNote);
    }

private:
    SpectralOscillatorFrameRenderer renderer;
    SpectralOscillatorRegionRuntime runtime;
    OscillatorRegionTraversalRenderer traversalRenderer;
};

}

std::unique_ptr<PreparedOscillatorRegion> prepareOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const CompiledVoiceContext& context,
        const AudioExecutionSpec& spec,
        int maximumCycleSamples,
        const std::vector<NodeAudioProcessor*>& processors) {
    if (ChainedOscillatorRecipeRenderer::supports(plan, region)) {
        auto prepared = std::make_unique<PreparedChainedOscillatorRegion>();
        return prepared->prepare(
                    plan, region, context, spec, maximumCycleSamples, processors)
                ? std::unique_ptr<PreparedOscillatorRegion>(std::move(prepared))
                : nullptr;
    }
    if (SpectralOscillatorFrameRenderer::supports(plan, region)) {
        auto prepared = std::make_unique<PreparedSpectralOscillatorRegion>();
        return prepared->prepare(
                    plan, region, context, spec, maximumCycleSamples, processors)
                ? std::unique_ptr<PreparedOscillatorRegion>(std::move(prepared))
                : nullptr;
    }
    return nullptr;
}

bool supportsPreparedOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    return ChainedOscillatorRecipeRenderer::supports(plan, region)
            || SpectralOscillatorFrameRenderer::supports(plan, region);
}

}
