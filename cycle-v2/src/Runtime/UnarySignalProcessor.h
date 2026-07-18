#pragma once

#include "AudioProcessContextUtils.h"

#include <Array/Buffer.h>

namespace CycleV2 {

struct SignalProcessPosition {
    size_t channel {};
    size_t column {};
};

class IUnarySignalOperation {
public:
    virtual ~IUnarySignalOperation() = default;

    virtual void beginBlock(size_t frameCount) {}
    virtual void beginChannelBlock(size_t channelCount, size_t frameCount) {
        ignoreUnused(channelCount);
        beginBlock(frameCount);
    }
    virtual void beginTraversalGrid(size_t columns, size_t rows) {}
    virtual void beginChannelTraversalGrid(size_t channelCount, size_t columns, size_t rows) {
        ignoreUnused(channelCount);
        beginTraversalGrid(columns, rows);
    }
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
            size_t frameCount,
            const AudioProcessWorkArena* arena = nullptr) {
        output.domain = input.domain;
        output.channelLayout = input.channelLayout;

        copyPayloadBlockExpandingScalars(output, input, frameCount);
        const size_t channelCount = payloadChannelCount(output);
        operation.beginChannelBlock(channelCount, frameCount);
        for (size_t channel = 0; channel < channelCount; ++channel) {
            auto& block = channel == 0 ? output.block : output.secondaryBlock;
            operation.processBuffer(
                    { block.samples.data(), (int) block.samples.size() },
                    { channel, 0 });
        }

        processTraversalGrid(operation, output, input, arena);
    }

private:
    static void processTraversalGrid(
            IUnarySignalOperation& operation,
            SignalPayload& output,
            const SignalPayload& input,
            const AudioProcessWorkArena* arena) {
        if (!input.traversalGrid.isValid()) {
            output.traversalGrid = {};
            output.secondaryTraversalGrid.values.clear();
            output.secondaryTraversalGrid.metadata = {};
            output.secondaryTraversalGrid.columns = 0;
            output.secondaryTraversalGrid.rows = 0;
            return;
        }

        const size_t channelCount = payloadChannelCount(output);
        operation.beginChannelTraversalGrid(
                channelCount,
                input.traversalGrid.columns,
                input.traversalGrid.rows);
        for (size_t channel = 0; channel < channelCount; ++channel) {
            const auto& inputGrid = payloadTraversalGrid(input, channel);
            auto& outputGrid = payloadTraversalGrid(output, channel);
            if (!inputGrid.isValid()) {
                outputGrid = {};
                continue;
            }

            auto metadata = inputGrid.metadata;
            metadata.valueDomain = output.domain;
            configureTraversalGrid(
                    outputGrid,
                    inputGrid.columns,
                    inputGrid.rows,
                    metadata,
                    arena);
            outputGrid.values.assign(inputGrid.values.begin(), inputGrid.values.end());

        }

        for (size_t column = 0; column < input.traversalGrid.columns; ++column) {
            operation.beginTraversalColumn(column, input.traversalGrid.rows);
            for (size_t channel = 0; channel < channelCount; ++channel) {
                auto& outputGrid = payloadTraversalGrid(output, channel);
                if (!outputGrid.isValid()) {
                    continue;
                }
                const size_t offset = column * outputGrid.rows;
                operation.processBuffer(
                        {
                                outputGrid.values.data() + offset,
                                (int) outputGrid.rows
                        },
                        { channel, column });
            }
        }
        operation.endTraversalGrid();
    }
};

}
