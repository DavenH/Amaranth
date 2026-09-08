#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Util/Arithmetic.h>
#include <vector>

#include "Runtime/AudioProcessContextUtils.h"
#include "Runtime/AudioProcessorFactories.h"

namespace CycleV2 {

namespace {

void renderLayer(
        PortDomain domain,
        Buffer<float> source,
        Buffer<float> secondarySource,
        Buffer<float> left,
        Buffer<float> right,
        const PanConfiguration& configuration) {
    float leftPan {};
    float rightPan {};
    Arithmetic::getPans(configuration.pan, leftPan, rightPan);
    source.copyTo(left);
    secondarySource.copyTo(right);
    if (domain == PortDomain::SpectralMagnitudeSignal && configuration.multiplicative) {
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(left, leftPan);
        CycleDsp::SpectralLayerCore::applyMultiplicativePan(right, rightPan);
    } else {
        left.mul(leftPan);
        right.mul(rightPan);
    }
}

void renderTraversalGrid(
        PortDomain domain,
        const SignalTraversalGrid& source,
        const SignalTraversalGrid& secondarySource,
        SignalTraversalGrid& left,
        SignalTraversalGrid& right,
        const PanConfiguration& configuration) {
    for (size_t column = 0; column < source.columns; ++column) {
        const int offset = (int) (column * source.rows);
        const int rowCount = (int) source.rows;
        renderLayer(
                domain,
                {
                        const_cast<float*>(source.values.data()) + offset,
                        rowCount
                },
                {
                        const_cast<float*>(secondarySource.values.data()) + offset,
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
                const PanConfiguration>(published.value);
    }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        sourceBlock.reserve(spec.maximumFrameCount);
        secondarySourceBlock.reserve(spec.maximumFrameCount);
    }

    const SignalTraversalGrid* probeTraversalGrid(
            const AudioProcessContext& context,
            size_t outputIndex) const override {
        if (outputIndex != 0) {
            return nullptr;
        }

        const SignalPayload* input = !context.inputViews.empty()
                ? context.inputViews.front()
                : (!context.inputs.empty() ? &context.inputs.front() : nullptr);
        return input != nullptr && input->traversalGrid.isValid()
                ? &input->traversalGrid
                : nullptr;
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
        copyBlockExpandingScalars(
                secondarySourceBlock,
                input->isStereo() ? input->secondaryBlock : input->block,
                context.frameCount);

        renderLayer(
                output.domain,
                {
                        sourceBlock.data(),
                        (int) sourceBlock.size()
                },
                {
                        secondarySourceBlock.data(),
                        (int) secondarySourceBlock.size()
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
            const SignalTraversalGrid& secondaryInput = input->isStereo()
                            && input->secondaryTraversalGrid.isValid()
                    ? input->secondaryTraversalGrid
                    : input->traversalGrid;
            renderTraversalGrid(
                    output.domain,
                    input->traversalGrid,
                    secondaryInput,
                    output.traversalGrid,
                    output.secondaryTraversalGrid,
                    *configuration);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
    std::shared_ptr<const PanConfiguration> configuration;
    std::vector<float> sourceBlock;
    std::vector<float> secondarySourceBlock;
};

}

std::unique_ptr<NodeAudioProcessor> createSpectralLayerAudioProcessor() {
    return std::make_unique<SpectralLayerAudioProcessor>();
}

}
