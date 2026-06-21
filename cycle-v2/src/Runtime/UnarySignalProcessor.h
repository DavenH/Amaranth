#pragma once

#include "AudioProcessTypes.h"

#include <Array/Buffer.h>
#include <Array/VecOps.h>

#include <algorithm>

namespace CycleV2 {

struct SignalProcessPosition {
    size_t column {};
};

template<class Processor>
class UnarySignalProcessor {
public:
    void process(
            SignalPayload& output,
            const SignalPayload& input,
            const std::vector<NodeParameter>& parameters,
            size_t frameCount) {
        auto& processor = static_cast<Processor&>(*this);
        processor.prepareProcess(parameters);

        output.domain = input.domain;
        output.channelLayout = input.channelLayout;

        copyInputBlock(output, input, frameCount);
        processor.processBuffer(
                { output.block.samples.data(), (int) output.block.samples.size() },
                {});

        processTraversalGrid(output, input, processor);
    }

protected:
    static void copyInputBlock(SignalPayload& output, const SignalPayload& input, size_t frameCount) {
        output.block.samples.resize(frameCount);

        Buffer<float> outputBuffer(output.block.samples.data(), (int) output.block.samples.size());
        if (input.block.samples.empty()) {
            outputBuffer.zero();
            return;
        }

        if (input.block.samples.size() == 1) {
            outputBuffer.set(input.block.samples.front());
            return;
        }

        const size_t copyCount = std::min(input.block.samples.size(), output.block.samples.size());
        VecOps::copy(input.block.samples.data(), output.block.samples.data(), (int) copyCount);

        if (copyCount < output.block.samples.size()) {
            outputBuffer
                    .sectionAtMost((int) copyCount, (int) (output.block.samples.size() - copyCount))
                    .zero();
        }
    }

private:
    static void processTraversalGrid(
            SignalPayload& output,
            const SignalPayload& input,
            Processor& processor) {
        if (!input.traversalGrid.isValid()) {
            output.traversalGrid = {};
            return;
        }

        output.traversalGrid.columns = input.traversalGrid.columns;
        output.traversalGrid.rows = input.traversalGrid.rows;
        output.traversalGrid.values = input.traversalGrid.values;

        for (size_t column = 0; column < output.traversalGrid.columns; ++column) {
            const size_t offset = column * output.traversalGrid.rows;
            processor.processBuffer(
                    {
                            output.traversalGrid.values.data() + offset,
                            (int) output.traversalGrid.rows
                    },
                    { column });
        }
    }
};

}
