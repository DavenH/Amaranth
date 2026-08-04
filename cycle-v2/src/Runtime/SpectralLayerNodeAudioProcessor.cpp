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

void renderTraversalGrid(
        PortDomain domain,
        const SignalTraversalGrid& source,
        SignalTraversalGrid& left,
        SignalTraversalGrid& right,
        const SpectralLayerConfiguration& configuration) {
    for (size_t column = 0; column < source.columns; ++column) {
        const int offset = (int) (column * source.rows);
        const int rowCount = (int) source.rows;
        renderLayer(
                domain,
                {
                        const_cast<float*>(source.values.data()) + offset,
                        rowCount
                },
                { left.values.data() + offset, rowCount },
                { right.values.data() + offset, rowCount },
                configuration);
    }
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
            renderTraversalGrid(
                    output.domain,
                    input->traversalGrid,
                    output.traversalGrid,
                    output.secondaryTraversalGrid,
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
