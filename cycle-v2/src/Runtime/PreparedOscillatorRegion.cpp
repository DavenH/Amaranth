#include <algorithm>

#include "Runtime/PreparedOscillatorRegion.h"
#include "Runtime/ChainedOscillatorRecipeRenderer.h"
#include "Runtime/ChainedOscillatorRegionRuntime.h"
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

bool regionContainsNode(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const String& nodeId) {
    return std::any_of(
            region.stepIndices.begin(),
            region.stepIndices.end(),
            [&](int stepIndex) {
                return stepIndex >= 0
                        && stepIndex < (int) plan.steps.size()
                        && plan.steps[(size_t) stepIndex].nodeId == nodeId;
            });
}

bool hasExternalProcessorConsumer(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region) {
    return std::any_of(
            plan.signalEdges.begin(),
            plan.signalEdges.end(),
            [&](const Edge& edge) {
                if (!regionContainsNode(plan, region, edge.sourceNodeId)
                        || regionContainsNode(plan, region, edge.destNodeId)) {
                    return false;
                }
                const auto destination = std::find_if(
                        plan.steps.begin(),
                        plan.steps.end(),
                        [&](const GraphExecutionStep& step) {
                            return step.nodeId == edge.destNodeId;
                        });
                return destination != plan.steps.end() && !destination->outputSink;
            });
}

class PreparedChainedOscillatorRegion final : public PreparedOscillatorRegion {
public:
    bool replacesDiagnosticProcessors() const override { return replaceDiagnostics; }

    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const CompiledVoiceContext& context,
            const AudioExecutionSpec& spec,
            int maximumCycleSamples,
            const std::vector<NodeAudioProcessor*>& processors) {
        const bool hasScratchAttachment = std::any_of(
                plan.steps.begin(),
                plan.steps.end(),
                [](const GraphExecutionStep& step) {
                    return std::any_of(
                            step.attachments.begin(),
                            step.attachments.end(),
                            [](const GraphStepAttachment& attachment) {
                                return attachment.destPortId == "scratch";
                            });
                });
        replaceDiagnostics = !hasScratchAttachment
                && !hasExternalProcessorConsumer(plan, region);
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

private:
    bool replaceDiagnostics { true };
    ChainedOscillatorRegionRuntime runtime;
    std::unique_ptr<OscillatorCycleRenderer> renderer;
};

class PreparedSpectralOscillatorRegion final : public PreparedOscillatorRegion {
public:
    bool replacesDiagnosticProcessors() const override { return false; }
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
                        context.lanes);
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
