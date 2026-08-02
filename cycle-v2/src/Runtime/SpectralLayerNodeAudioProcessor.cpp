#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <vector>

#include "AudioProcessContextUtils.h"
#include "AudioProcessorFactories.h"

namespace CycleV2 {

namespace {

void renderLayer(
        PortDomain domain,
        Buffer<float> source,
        Buffer<float> left,
        Buffer<float> right,
        const SpectralLayerConfiguration& configuration) {
    if (domain == PortDomain::SpectralPhaseSignal) {
        CycleDsp::SpectralLayerCore::renderPhaseChannels(
                source,
                left,
                right,
                configuration.pan,
                configuration.range);
        return;
    }

    if (domain == PortDomain::SpectralMagnitudeSignal) {
        CycleDsp::SpectralLayerCore::renderMagnitudeChannels(
                source,
                left,
                right,
                configuration.pan,
                configuration.range,
                configuration.additive);
        return;
    }

    left.zero();
    right.zero();
}

class SpectralLayerAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::SpectralLayer; }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<
                const SpectralLayerConfiguration>(published.value);
    }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        sourceBlock.reserve(spec.maximumFrameCount);
    }

    void process(AudioProcessContext& context) override {
        const SignalPayload* input = inputAt(context, 0);
        if (input == nullptr || configuration == nullptr) {
            clearOutput(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        output.domain = input->domain;
        output.channelLayout = ChannelLayout::StereoPair;
        output.block.samples.resize(context.frameCount);
        output.secondaryBlock.samples.resize(context.frameCount);
        if (context.workArena != nullptr) {
            context.workArena->reserve(output);
        }
        copyBlockExpandingScalars(sourceBlock, input->block, context.frameCount);

        renderLayer(
                output.domain,
                {
                        sourceBlock.data(),
                        (int) sourceBlock.size()
                },
                payloadBuffer(output, context.frameCount),
                payloadBuffer(output, 1, context.frameCount),
                *configuration);

        if (input->traversalGrid.isValid()) {
            configureTraversalGrid(
                    output.traversalGrid,
                    input->traversalGrid.columns,
                    input->traversalGrid.rows,
                    input->traversalGrid.metadata,
                    context.workArena);
            configureTraversalGrid(
                    output.secondaryTraversalGrid,
                    input->traversalGrid.columns,
                    input->traversalGrid.rows,
                    input->traversalGrid.metadata,
                    context.workArena);
            renderLayer(
                    output.domain,
                    {
                            const_cast<float*>(input->traversalGrid.values.data()),
                            (int) input->traversalGrid.values.size()
                    },
                    {
                            output.traversalGrid.values.data(),
                            (int) output.traversalGrid.values.size()
                    },
                    {
                            output.secondaryTraversalGrid.values.data(),
                            (int) output.secondaryTraversalGrid.values.size()
                    },
                    *configuration);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
    std::shared_ptr<const SpectralLayerConfiguration> configuration;
    std::vector<float> sourceBlock;
};

}

std::unique_ptr<NodeAudioProcessor> createSpectralLayerAudioProcessor() {
    return std::make_unique<SpectralLayerAudioProcessor>();
}

}
