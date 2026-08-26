#include "Nodes/FFT/FftSignalProcessor.h"

#include "Graph/NodeParameterMap.h"

#include <algorithm>

namespace CycleV2 {

namespace {

class TraversalGridColumnReader {
public:
    explicit TraversalGridColumnReader(const SignalTraversalGrid& source) : grid(&source) {}
    explicit TraversalGridColumnReader(const SignalTraversalGrid* source) : grid(source) {}

    size_t columns() const { return grid != nullptr ? grid->columns : 0; }
    size_t rows() const { return grid != nullptr ? grid->rows : 0; }

    bool isFrequencyCompanionFor(const TraversalGridColumnReader& other) const {
        return grid != nullptr
                && grid->isValid()
                && columns() == other.columns()
                && rows() == other.rows()
                && grid->metadata.rowAxis == TraversalGridAxis::Frequency;
    }

    void read(size_t column, AudioProcessBlock& destination) const {
        destination.samples.resize(rows());
        copyTraversalGridColumn(
                { destination.samples.data(), (int) destination.samples.size() },
                *grid,
                column);
    }

private:
    const SignalTraversalGrid* grid;
};

class TraversalGridColumnWriter {
public:
    TraversalGridColumnWriter(
            SignalTraversalGrid& destination,
            size_t columns,
            size_t rows,
            TraversalGridMetadata metadata,
            const AudioProcessWorkArena* arena) : grid(destination) {
        configureTraversalGrid(grid, columns, rows, metadata, arena);
    }

    void write(size_t column, const AudioProcessBlock& source) {
        const size_t count = std::min(grid.rows, source.samples.size());
        Buffer<float> destination(
                grid.values.data() + column * grid.rows,
                (int) count);
        Buffer<float> input(
                const_cast<float*>(source.samples.data()),
                (int) count);
        VecOps::copy(input.get(), destination.get(), destination.size());
    }

private:
    SignalTraversalGrid& grid;
};

bool inverseUsesHalfCycleCarry(const std::vector<NodeParameter>& parameters) {
    return NodeParameterMap(parameters).stringValue("mode", "cyclic") == "acyclicCarry";
}

}

void FftSignalProcessor::prepareExecution(size_t maximumFrameCount) {
    for (auto& channelProcessors : preparedBlockwiseDsp) {
        channelProcessors.clear();
        for (size_t frameCount = 1; frameCount <= maximumFrameCount; frameCount *= 2) {
            auto processor = std::make_unique<FftBlockwiseDsp>();
            processor->prepare(frameCount);
            channelProcessors.push_back(std::move(processor));
            if (frameCount > maximumFrameCount / 2) {
                break;
            }
        }
    }
}

FftBlockwiseDsp& FftSignalProcessor::blockwiseFor(
        size_t frameCount,
        size_t channel) {
    const size_t channelIndex = std::min(channel, blockwiseDsp.size() - 1);
    if (frameCount > 0 && (frameCount & (frameCount - 1)) == 0) {
        size_t exponent = 0;
        for (size_t value = frameCount; value > 1; value /= 2) {
            ++exponent;
        }
        if (exponent < preparedBlockwiseDsp[channelIndex].size()) {
            return *preparedBlockwiseDsp[channelIndex][exponent];
        }
    }
    return blockwiseDsp[channelIndex];
}

void FftSignalProcessor::processForward(AudioProcessContext& context) {
    SignalPayload* input = inputAt(context, 0);

    if (input == nullptr) {
        clearOutput(context);
        return;
    }

    auto magnitude = makeOutputPayload(context, 0);
    auto phase = makeOutputPayload(context, 1);
    const bool stereo = input->isStereo() || magnitude.isStereo() || phase.isStereo();
    const size_t channelCount = stereo ? 2u : 1u;
    if (stereo) {
        magnitude.channelLayout = ChannelLayout::StereoPair;
        phase.channelLayout = ChannelLayout::StereoPair;
        magnitude.secondaryBlock.samples.resize(context.frameCount);
        phase.secondaryBlock.samples.resize(context.frameCount);
    }
    for (size_t channel = 0; channel < channelCount; ++channel) {
        const size_t inputChannel = input->isStereo() ? channel : 0;
        blockwiseFor(input->block.samples.size(), channel).forward(
                payloadBlock(*input, inputChannel),
                payloadBlock(magnitude, channel),
                payloadBlock(phase, channel));
        publishForwardTraversalGrids(
                *input,
                magnitude,
                phase,
                inputChannel,
                channel,
                context.workArena);
    }

    publishOutputs(context, std::move(magnitude), std::move(phase));
}

void FftSignalProcessor::processInverse(AudioProcessContext& context) {
    processInverse(context, inverseUsesHalfCycleCarry(processParameters(context)));
}

void FftSignalProcessor::processInverse(AudioProcessContext& context, bool useHalfCycleCarry) {
    SignalPayload* magnitude = inputAt(context, 0);

    if (magnitude == nullptr) {
        clearOutput(context);
        return;
    }

    auto output = makeOutputPayload(context, 0);
    SignalPayload* phase = inputAt(context, 1);
    const bool stereo = magnitude->isStereo()
            || (phase != nullptr && phase->isStereo());
    if (stereo) {
        output.channelLayout = ChannelLayout::StereoPair;
        output.secondaryBlock.samples.resize(context.frameCount);
    }
    const size_t channelCount = stereo ? 2u : 1u;
    for (size_t channel = 0; channel < channelCount; ++channel) {
        auto& blockwise = blockwiseFor(output.block.samples.size(), channel);
        blockwise.setHalfCycleCarryEnabled(useHalfCycleCarry);
        const size_t magnitudeChannel = magnitude->isStereo() ? channel : 0;
        const size_t phaseChannel = phase != nullptr && phase->isStereo() ? channel : 0;
        const SignalBlock* phaseBlock = phase != nullptr
                ? &payloadBlock(*phase, phaseChannel)
                : nullptr;
        blockwise.inverse(
                payloadBlock(*magnitude, magnitudeChannel),
                phaseBlock,
                payloadBlock(output, channel));
        publishInverseTraversalGrid(
                *magnitude,
                phase,
                output,
                channel,
                useHalfCycleCarry,
                context.workArena);
    }
    publishSingleOutput(context, std::move(output));
}

void FftSignalProcessor::publishForwardTraversalGrids(
        const SignalPayload& input,
        SignalPayload& magnitude,
        SignalPayload& phase,
        size_t inputChannel,
        size_t outputChannel,
        const AudioProcessWorkArena* arena) {
    const auto& inputGrid = payloadTraversalGrid(input, inputChannel);
    if (!inputGrid.isValid()) {
        return;
    }

    TraversalGridColumnReader inputColumns(inputGrid);
    inputColumns.read(0, scratchTimeColumn);
    traversalDsp.resetState();
    traversalDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);

    const size_t binRows = scratchMagnitudeColumn.samples.size();
    if (binRows == 0 || scratchPhaseColumn.samples.size() < binRows) {
        return;
    }

    TraversalGridColumnWriter magnitudeColumns(
            payloadTraversalGrid(magnitude, outputChannel),
            inputGrid.columns,
            binRows,
            frequencyMetadataFor(inputGrid, magnitude, binRows),
            arena);
    TraversalGridColumnWriter phaseColumns(
            payloadTraversalGrid(phase, outputChannel),
            inputGrid.columns,
            binRows,
            frequencyMetadataFor(inputGrid, phase, binRows),
            arena);
    magnitudeColumns.write(0, scratchMagnitudeColumn);
    phaseColumns.write(0, scratchPhaseColumn);

    for (size_t column = 1; column < inputColumns.columns(); ++column) {
        inputColumns.read(column, scratchTimeColumn);
        traversalDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);
        magnitudeColumns.write(column, scratchMagnitudeColumn);
        phaseColumns.write(column, scratchPhaseColumn);
    }

}

TraversalGridMetadata FftSignalProcessor::frequencyMetadataFor(
        const SignalTraversalGrid& inputGrid,
        const SignalPayload& output,
        size_t rows) const {
    auto metadata = makeTraversalGridMetadata(
            output.domain,
            inputGrid.columns,
            rows,
            inputGrid.metadata.columnAxis,
            TraversalGridAxis::Frequency);
    metadata.columnResolution = inputGrid.metadata.columnResolution;
    metadata.frequencySampling = TraversalGridFrequencySampling::LinearBins;
    metadata.frequencyMidiNote = inputGrid.metadata.frequencyMidiNote;
    return metadata;
}

void FftSignalProcessor::publishInverseTraversalGrid(
        const SignalPayload& magnitude,
        const SignalPayload* phase,
        SignalPayload& output,
        size_t channel,
        bool useHalfCycleCarry,
        const AudioProcessWorkArena* arena) {
    const size_t magnitudeChannel = magnitude.isStereo() ? channel : 0;
    const auto& magnitudeGrid = payloadTraversalGrid(magnitude, magnitudeChannel);
    if (!magnitudeGrid.isValid()) {
        return;
    }

    auto metadata = makeTraversalGridMetadata(
            output.domain,
            magnitudeGrid.columns,
            output.block.samples.size(),
            magnitudeGrid.metadata.columnAxis,
            TraversalGridAxis::Time);
    metadata.columnResolution = magnitudeGrid.metadata.columnResolution;
    TraversalGridColumnReader magnitudeColumns(magnitudeGrid);
    TraversalGridColumnWriter outputColumns(
            payloadTraversalGrid(output, channel),
            magnitudeGrid.columns,
            output.block.samples.size(),
            metadata,
            arena);

    const size_t phaseChannel = phase != nullptr && phase->isStereo() ? channel : 0;
    TraversalGridColumnReader candidatePhaseColumns(phase != nullptr
            ? &payloadTraversalGrid(*phase, phaseChannel)
            : nullptr);
    const bool hasCompatiblePhase = candidatePhaseColumns.isFrequencyCompanionFor(
            magnitudeColumns);

    traversalDsp.setHalfCycleCarryEnabled(useHalfCycleCarry);
    traversalDsp.resetState();
    scratchOutputColumn.samples.resize(output.block.samples.size());
    for (size_t column = 0; column < magnitudeColumns.columns(); ++column) {
        magnitudeColumns.read(column, scratchMagnitudeColumn);

        const AudioProcessBlock* phaseColumnPtr = nullptr;
        if (hasCompatiblePhase) {
            candidatePhaseColumns.read(column, scratchPhaseColumn);
            phaseColumnPtr = &scratchPhaseColumn;
        }

        traversalDsp.inverse(scratchMagnitudeColumn, phaseColumnPtr, scratchOutputColumn);
        outputColumns.write(column, scratchOutputColumn);
    }
}

}
