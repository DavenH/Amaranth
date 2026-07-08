#include "FftSignalProcessor.h"

#include <algorithm>

namespace CycleV2 {

void FftSignalProcessor::processForward(AudioProcessContext& context) {
    SignalPayload* input = inputAt(context, 0);

    if (input == nullptr) {
        clearOutput(context);
        return;
    }

    auto magnitude = makeOutputPayload(context, 0);
    auto phase = makeOutputPayload(context, 1);
    blockwiseDsp.forward(input->block, magnitude.block, phase.block);
    publishForwardTraversalGrids(*input, magnitude, phase, context.workArena);

    publishOutputs(context, std::move(magnitude), std::move(phase));
}

void FftSignalProcessor::processInverse(AudioProcessContext& context) {
    SignalPayload* magnitude = inputAt(context, 0);

    if (magnitude == nullptr) {
        clearOutput(context);
        return;
    }

    auto output = makeOutputPayload(context, 0);
    SignalPayload* phase = inputAt(context, 1);
    blockwiseDsp.inverse(magnitude->block, phase != nullptr ? &phase->block : nullptr, output.block);
    publishInverseTraversalGrid(*magnitude, phase, output, context.workArena);
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
    copyGridColumnToBlock(input.traversalGrid, 0, scratchTimeColumn);
    blockwiseDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);

    if (scratchMagnitudeColumn.samples.empty() || scratchPhaseColumn.samples.empty()) {
        return;
    }

    configureTraversalGrid(
            magnitude.traversalGrid,
            inputGrid.columns,
            scratchMagnitudeColumn.samples.size(),
            frequencyMetadataFor(inputGrid, magnitude, scratchMagnitudeColumn.samples.size()),
            arena);
    configureTraversalGrid(
            phase.traversalGrid,
            inputGrid.columns,
            scratchPhaseColumn.samples.size(),
            frequencyMetadataFor(inputGrid, phase, scratchPhaseColumn.samples.size()),
            arena);
    copyBlockToGridColumn(scratchMagnitudeColumn, magnitude.traversalGrid, 0);
    copyBlockToGridColumn(scratchPhaseColumn, phase.traversalGrid, 0);

    for (size_t column = 1; column < inputGrid.columns; ++column) {
        copyGridColumnToBlock(inputGrid, column, scratchTimeColumn);
        blockwiseDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);
        copyBlockToGridColumn(scratchMagnitudeColumn, magnitude.traversalGrid, column);
        copyBlockToGridColumn(scratchPhaseColumn, phase.traversalGrid, column);
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

void FftSignalProcessor::copyGridColumnToBlock(
        const SignalTraversalGrid& grid,
        size_t column,
        AudioProcessBlock& block) const {
    block.samples.resize(grid.rows);
    std::copy(
            grid.values.begin() + (ptrdiff_t) (column * grid.rows),
            grid.values.begin() + (ptrdiff_t) ((column + 1) * grid.rows),
            block.samples.begin());
}

void FftSignalProcessor::copyBlockToGridColumn(
        const AudioProcessBlock& block,
        SignalTraversalGrid& grid,
        size_t column) const {
    const size_t count = std::min(grid.rows, block.samples.size());
    std::copy(
            block.samples.begin(),
            block.samples.begin() + (ptrdiff_t) count,
            grid.values.begin() + (ptrdiff_t) (column * grid.rows));
}

void FftSignalProcessor::publishInverseTraversalGrid(
        const SignalPayload& magnitude,
        const SignalPayload* phase,
        SignalPayload& output,
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
    configureTraversalGrid(
            output.traversalGrid,
            magnitude.traversalGrid.columns,
            output.block.samples.size(),
            metadata,
            arena);

    for (size_t column = 0; column < magnitude.traversalGrid.columns; ++column) {
        copyGridColumnToBlock(magnitude.traversalGrid, column, scratchMagnitudeColumn);
        scratchOutputColumn.samples.resize(output.block.samples.size());

        const AudioProcessBlock* phaseColumnPtr = nullptr;
        if (phase != nullptr && phase->traversalGrid.isValid()
                && phase->traversalGrid.columns == magnitude.traversalGrid.columns
                && phase->traversalGrid.rows == magnitude.traversalGrid.rows
                && phase->traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency) {
            copyGridColumnToBlock(phase->traversalGrid, column, scratchPhaseColumn);
            phaseColumnPtr = &scratchPhaseColumn;
        }

        blockwiseDsp.inverse(scratchMagnitudeColumn, phaseColumnPtr, scratchOutputColumn);
        copyBlockToGridColumn(scratchOutputColumn, output.traversalGrid, column);
    }
}

}
