#pragma once

#include "AudioProcessContextUtils.h"

#include <Array/Buffer.h>

namespace CycleV2 {

struct SignalProcessPosition {
    size_t column {};
};

class IUnarySignalOperation {
public:
    virtual ~IUnarySignalOperation() = default;

    virtual void prepareProcess(
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing) = 0;
    virtual void beginBlock(size_t frameCount) {}
    virtual void beginTraversalGrid(size_t columns, size_t rows) {}
    virtual void beginTraversalColumn(size_t column, size_t rows) {}
    virtual void endTraversalGrid() {}
    virtual void processBuffer(Buffer<float> buffer, const SignalProcessPosition& position) = 0;
};

class UnarySignalProcessor {
public:
    void process(
            IUnarySignalOperation& operation,
            SignalPayload& output,
            const SignalPayload& input,
            const std::vector<NodeParameter>& parameters,
            const AudioProcessTiming& timing,
            size_t frameCount) {
        operation.prepareProcess(parameters, timing);

        output.domain = input.domain;
        output.channelLayout = input.channelLayout;

        copyPayloadBlockExpandingScalars(output, input, frameCount);
        operation.beginBlock(output.block.samples.size());
        operation.processBuffer(
                { output.block.samples.data(), (int) output.block.samples.size() },
                {});

        processTraversalGrid(operation, output, input);
    }

private:
    static void processTraversalGrid(
            IUnarySignalOperation& operation,
            SignalPayload& output,
            const SignalPayload& input) {
        if (!input.traversalGrid.isValid()) {
            output.traversalGrid = {};
            return;
        }

        output.traversalGrid.columns = input.traversalGrid.columns;
        output.traversalGrid.rows = input.traversalGrid.rows;
        output.traversalGrid.values = input.traversalGrid.values;

        operation.beginTraversalGrid(output.traversalGrid.columns, output.traversalGrid.rows);
        for (size_t column = 0; column < output.traversalGrid.columns; ++column) {
            const size_t offset = column * output.traversalGrid.rows;
            operation.beginTraversalColumn(column, output.traversalGrid.rows);
            operation.processBuffer(
                    {
                            output.traversalGrid.values.data() + offset,
                            (int) output.traversalGrid.rows
                    },
                    { column });
        }
        operation.endTraversalGrid();
    }
};

}
