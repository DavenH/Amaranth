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
    publishForwardTraversalGrids(*input, magnitude, phase);

    publishOutputs(context, { std::move(magnitude), std::move(phase) });
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
    publishInverseTraversalGrid(*magnitude, phase, output);
    publishSingleOutput(context, std::move(output));
}

void FftSignalProcessor::publishForwardTraversalGrids(
        const SignalPayload& input,
        SignalPayload& magnitude,
        SignalPayload& phase) {
    if (!input.traversalGrid.isValid()) {
        return;
    }

    std::vector<AudioProcessBlock> timeColumns;
    timeColumns.reserve(input.traversalGrid.columns);

    for (size_t column = 0; column < input.traversalGrid.columns; ++column) {
        AudioProcessBlock timeColumn;
        timeColumn.samples.assign(
                input.traversalGrid.values.begin() + (ptrdiff_t) (column * input.traversalGrid.rows),
                input.traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * input.traversalGrid.rows));
        timeColumns.push_back(std::move(timeColumn));
    }

    const auto columns = gridwiseDsp.forwardColumns(timeColumns);
    publishForwardGridResult(columns, magnitude, true);
    publishForwardGridResult(columns, phase, false);
}

void FftSignalProcessor::publishForwardGridResult(
        const std::vector<FftGridColumn>& columns,
        SignalPayload& output,
        bool magnitude) const {
    if (columns.empty()) {
        return;
    }

    const auto& first = magnitude ? columns.front().magnitude : columns.front().phase;
    if (first.block.samples.empty()) {
        return;
    }

    configureTraversalGrid(output.traversalGrid, columns.size(), first.block.samples.size());

    for (size_t column = 0; column < columns.size(); ++column) {
        const auto& payload = magnitude ? columns[column].magnitude : columns[column].phase;
        const auto& samples = payload.block.samples;
        const size_t count = std::min(output.traversalGrid.rows, samples.size());
        std::copy(
                samples.begin(),
                samples.begin() + (ptrdiff_t) count,
                output.traversalGrid.values.begin() + (ptrdiff_t) (column * output.traversalGrid.rows));
    }
}

void FftSignalProcessor::publishInverseTraversalGrid(
        const SignalPayload& magnitude,
        const SignalPayload* phase,
        SignalPayload& output) {
    if (!magnitude.traversalGrid.isValid()) {
        return;
    }

    configureTraversalGrid(
            output.traversalGrid,
            magnitude.traversalGrid.columns,
            output.block.samples.size());

    for (size_t column = 0; column < magnitude.traversalGrid.columns; ++column) {
        AudioProcessBlock magnitudeColumn;
        AudioProcessBlock phaseColumn;
        AudioProcessBlock outputColumn;
        magnitudeColumn.samples.assign(
                magnitude.traversalGrid.values.begin() + (ptrdiff_t) (column * magnitude.traversalGrid.rows),
                magnitude.traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * magnitude.traversalGrid.rows));
        outputColumn.samples.resize(output.block.samples.size());

        const AudioProcessBlock* phaseColumnPtr = nullptr;
        if (phase != nullptr && phase->traversalGrid.isValid()
                && phase->traversalGrid.columns == magnitude.traversalGrid.columns) {
            phaseColumn.samples.assign(
                    phase->traversalGrid.values.begin() + (ptrdiff_t) (column * phase->traversalGrid.rows),
                    phase->traversalGrid.values.begin() + (ptrdiff_t) ((column + 1) * phase->traversalGrid.rows));
            phaseColumnPtr = &phaseColumn;
        }

        blockwiseDsp.inverse(magnitudeColumn, phaseColumnPtr, outputColumn);
        const size_t count = std::min(output.traversalGrid.rows, outputColumn.samples.size());
        std::copy(
                outputColumn.samples.begin(),
                outputColumn.samples.begin() + (ptrdiff_t) count,
                output.traversalGrid.values.begin() + (ptrdiff_t) (column * output.traversalGrid.rows));
    }
}

}
