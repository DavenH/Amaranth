#include "PreparedOscillatorRegion.h"

#include "ChainedOscillatorRecipeRenderer.h"
#include "ChainedOscillatorRegionRuntime.h"
#include "SpectralOscillatorFrameRenderer.h"
#include "SpectralOscillatorRegionRuntime.h"
#include "../Graph/GraphCompiler.h"

#include <Util/Arithmetic.h>

namespace CycleV2 {

namespace {

class PreparedChainedOscillatorRegion final : public PreparedOscillatorRegion {
public:
    bool replacesDiagnosticProcessors() const override { return true; }

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const CompiledVoiceContext& context,
            const AudioExecutionSpec& spec,
            int maximumCycleSamples) {
        auto preparedRenderer = std::make_unique<ChainedOscillatorRecipeRenderer>();
        if (!preparedRenderer->prepare(plan, region, maximumCycleSamples)
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

    bool process(
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right) override {
        return runtime.process(
                midiNote,
                velocity,
                pitchEnvelope,
                left,
                right,
                *renderer);
    }

private:
    ChainedOscillatorRegionRuntime runtime;
    std::unique_ptr<OscillatorCycleRenderer> renderer;
};

class PreparedSpectralOscillatorRegion final : public PreparedOscillatorRegion {
public:
    bool replacesDiagnosticProcessors() const override { return false; }

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const CompiledVoiceContext& context,
            const AudioExecutionSpec& spec,
            int maximumCycleSamples) {
        const int maximumFixedFrameSize = Arithmetic::getNextPow2(
                (float) maximumCycleSamples);
        return renderer.prepare(plan, region, maximumFixedFrameSize)
                && runtime.prepare(
                        spec.maximumFrameCount,
                        maximumCycleSamples,
                        maximumFixedFrameSize,
                        spec.sampleRate,
                        context.lanes);
    }

    void reset() override {
        runtime.reset();
        renderer.reset();
    }

    bool process(
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right) override {
        return runtime.process(
                midiNote,
                velocity,
                pitchEnvelope,
                left,
                right,
                renderer);
    }

private:
    SpectralOscillatorFrameRenderer renderer;
    SpectralOscillatorRegionRuntime runtime;
};

}

std::unique_ptr<PreparedOscillatorRegion> prepareOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const CompiledVoiceContext& context,
        const AudioExecutionSpec& spec,
        int maximumCycleSamples) {
    if (ChainedOscillatorRecipeRenderer::supports(plan, region)) {
        auto prepared = std::make_unique<PreparedChainedOscillatorRegion>();
        return prepared->prepare(plan, region, context, spec, maximumCycleSamples)
                ? std::unique_ptr<PreparedOscillatorRegion>(std::move(prepared))
                : nullptr;
    }
    if (SpectralOscillatorFrameRenderer::supports(plan, region)) {
        auto prepared = std::make_unique<PreparedSpectralOscillatorRegion>();
        return prepared->prepare(plan, region, context, spec, maximumCycleSamples)
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
