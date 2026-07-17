#include "FftSignalProcessor.h"

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
    return typedParameterString(parameters, "mode", "cyclic") == "acyclicCarry";
}

}

void FftSignalProcessor::prepareExecution(size_t maximumFrameCount) {
    preparedBlockwiseDsp.clear();
    for (size_t frameCount = 1; frameCount <= maximumFrameCount; frameCount *= 2) {
        auto processor = std::make_unique<FftBlockwiseDsp>();
        processor->prepare(frameCount);
        preparedBlockwiseDsp.push_back(std::move(processor));
        if (frameCount > maximumFrameCount / 2) {
            break;
        }
    }
}

FftBlockwiseDsp& FftSignalProcessor::blockwiseFor(size_t frameCount) {
    if (frameCount > 0 && (frameCount & (frameCount - 1)) == 0) {
        size_t exponent = 0;
        for (size_t value = frameCount; value > 1; value /= 2) {
            ++exponent;
        }
        if (exponent < preparedBlockwiseDsp.size()) {
            return *preparedBlockwiseDsp[exponent];
        }
    }
    return blockwiseDsp;
}

void FftSignalProcessor::processForward(AudioProcessContext& context) {
    SignalPayload* input = inputAt(context, 0);

    if (input == nullptr) {
        clearOutput(context);
        return;
    }

    auto magnitude = makeOutputPayload(context, 0);
    auto phase = makeOutputPayload(context, 1);
    blockwiseFor(input->block.samples.size()).forward(input->block, magnitude.block, phase.block);
    publishForwardTraversalGrids(*input, magnitude, phase, context.workArena);

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
    auto& blockwise = blockwiseFor(output.block.samples.size());
    blockwise.setHalfCycleCarryEnabled(useHalfCycleCarry);
    blockwise.inverse(magnitude->block, phase != nullptr ? &phase->block : nullptr, output.block);
    publishInverseTraversalGrid(*magnitude, phase, output, useHalfCycleCarry, context.workArena);
    publishSingleOutput(context, std::move(output));
}

void FftSignalProcessor::publishForwardTraversalGrids(
        const SignalPayload& input,
        SignalPayload& magnitude,
        SignalPayload& phase,
        const AudioProcessWorkArena* arena) {
    if (!input.traversalGrid.isValid()) {
        return;
    }

    const auto& inputGrid = input.traversalGrid;
    TraversalGridColumnReader inputColumns(inputGrid);
    inputColumns.read(0, scratchTimeColumn);
    traversalDsp.resetState();
    traversalDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);

    const size_t binRows = scratchMagnitudeColumn.samples.size();
    if (binRows == 0 || scratchPhaseColumn.samples.size() < binRows) {
        return;
    }

    TraversalGridColumnWriter magnitudeColumns(
            magnitude.traversalGrid,
            inputGrid.columns,
            binRows,
            frequencyMetadataFor(inputGrid, magnitude, binRows),
            arena);
    TraversalGridColumnWriter phaseColumns(
            phase.traversalGrid,
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
    return metadata;
}

void FftSignalProcessor::publishInverseTraversalGrid(
        const SignalPayload& magnitude,
        const SignalPayload* phase,
        SignalPayload& output,
        bool useHalfCycleCarry,
        const AudioProcessWorkArena* arena) {
    if (!magnitude.traversalGrid.isValid()) {
        return;
    }

    auto metadata = makeTraversalGridMetadata(
            output.domain,
            magnitude.traversalGrid.columns,
            output.block.samples.size(),
            magnitude.traversalGrid.metadata.columnAxis,
            TraversalGridAxis::Time);
    metadata.columnResolution = magnitude.traversalGrid.metadata.columnResolution;
    TraversalGridColumnReader magnitudeColumns(magnitude.traversalGrid);
    TraversalGridColumnWriter outputColumns(
            output.traversalGrid,
            magnitude.traversalGrid.columns,
            output.block.samples.size(),
            metadata,
            arena);

    TraversalGridColumnReader candidatePhaseColumns(
            phase != nullptr ? &phase->traversalGrid : nullptr);
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
