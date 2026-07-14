#include "FftSignalProcessor.h"

#include <algorithm>

namespace CycleV2 {

namespace {

bool inverseUsesHalfCycleCarry(const std::vector<NodeParameter>& parameters) {
    return typedParameterString(parameters, "mode", "cyclic") == "acyclicCarry";
}

}

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
    const bool useHalfCycleCarry = inverseUsesHalfCycleCarry(context.parameters);
    blockwiseDsp.setHalfCycleCarryEnabled(useHalfCycleCarry);
    blockwiseDsp.inverse(magnitude->block, phase != nullptr ? &phase->block : nullptr, output.block);
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
    copyGridColumnToBlock(input.traversalGrid, 0, scratchTimeColumn);
    traversalDsp.resetState();
    traversalDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);

    const size_t binRows = scratchMagnitudeColumn.samples.size();
    if (binRows == 0 || scratchPhaseColumn.samples.size() < binRows) {
        return;
    }

    configureTraversalGrid(
            magnitude.traversalGrid,
            inputGrid.columns,
            binRows,
            frequencyMetadataFor(inputGrid, magnitude, binRows),
            arena);
    configureTraversalGrid(
            phase.traversalGrid,
            inputGrid.columns,
            binRows,
            frequencyMetadataFor(inputGrid, phase, binRows),
            arena);
    copyBlockToGridColumn(scratchMagnitudeColumn, magnitude.traversalGrid, 0);
    copyBlockToGridColumn(scratchPhaseColumn, phase.traversalGrid, 0);

    for (size_t column = 1; column < inputGrid.columns; ++column) {
        copyGridColumnToBlock(inputGrid, column, scratchTimeColumn);
        traversalDsp.forward(scratchTimeColumn, scratchMagnitudeColumn, scratchPhaseColumn);
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
    copyTraversalGridColumn({ block.samples.data(), (int) block.samples.size() }, grid, column);
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
    configureTraversalGrid(
            output.traversalGrid,
            magnitude.traversalGrid.columns,
            output.block.samples.size(),
            metadata,
            arena);

    traversalDsp.setHalfCycleCarryEnabled(useHalfCycleCarry);
    traversalDsp.resetState();
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

        traversalDsp.inverse(scratchMagnitudeColumn, phaseColumnPtr, scratchOutputColumn);
        copyBlockToGridColumn(scratchOutputColumn, output.traversalGrid, column);
    }
}

}
