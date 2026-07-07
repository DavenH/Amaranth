#pragma once

#include "AudioProcessTypes.h"

#include <Array/Buffer.h>
#include <Array/VecOps.h>

#include <algorithm>

namespace CycleV2 {

inline Buffer<float> blockBuffer(AudioProcessBlock& block, size_t frameCount) {
    return { block.samples.data(), (int) frameCount };
}

inline Buffer<float> payloadBuffer(SignalPayload& payload, size_t frameCount) {
    return blockBuffer(payload.block, frameCount);
}

inline void copyBlockExpandingScalars(Buffer<float> dest, const SignalBlock& source, size_t frameCount) {
    if (source.samples.empty()) {
        dest.zero();
        return;
    }

    if (source.samples.size() == 1) {
        dest.set(source.samples.front());
        return;
    }

    const size_t copyCount = std::min({ source.samples.size(), frameCount, (size_t) dest.size() });
    VecOps::copy(source.samples.data(), dest.get(), (int) copyCount);

    if (copyCount < (size_t) dest.size()) {
        dest.sectionAtMost((int) copyCount, (int) ((size_t) dest.size() - copyCount)).zero();
    }
}

inline void copyBlockExpandingScalars(
        std::vector<float>& dest,
        const SignalBlock& source,
        size_t frameCount) {
    dest.resize(frameCount);
    copyBlockExpandingScalars({ dest.data(), (int) dest.size() }, source, frameCount);
}

inline void copyPayloadBlockExpandingScalars(
        SignalPayload& dest,
        const SignalPayload& source,
        size_t frameCount) {
    copyBlockExpandingScalars(dest.block.samples, source.block, frameCount);
}

inline void copyTraversalGridColumn(Buffer<float> dest, const SignalTraversalGrid& grid, size_t column) {
    if (!grid.isValid() || column >= grid.columns) {
        dest.zero();
        return;
    }

    const size_t copyCount = std::min(grid.rows, (size_t) dest.size());
    VecOps::copy(grid.values.data() + column * grid.rows, dest.get(), (int) copyCount);

    if (copyCount < (size_t) dest.size()) {
        dest.sectionAtMost((int) copyCount, (int) ((size_t) dest.size() - copyCount)).zero();
    }
}

inline void copyTraversalGrid(SignalPayload& dest, const SignalTraversalGrid& source) {
    dest.traversalGrid = source;
}

inline SignalPayload makeOutputPayload(const AudioOutputPort& port, size_t frameCount) {
    SignalPayload payload;
    payload.domain = port.domain;
    payload.channelLayout = port.channelLayout;
    payload.block.samples.resize(frameCount);
    return payload;
}

inline SignalPayload makeOutputPayload(AudioProcessContext& context, size_t index) {
    if (index < context.outputPorts.size()) {
        return makeOutputPayload(context.outputPorts[index], context.frameCount);
    }

    SignalPayload payload;
    payload.block.samples.resize(context.frameCount);
    return payload;
}

inline void publishSingleOutput(AudioProcessContext& context, SignalPayload output) {
    context.outputs.clear();
    context.outputs.push_back(std::move(output));
}

inline void publishOutputs(AudioProcessContext& context, std::vector<SignalPayload> outputs) {
    context.outputs = std::move(outputs);
}

inline void configureTraversalGrid(SignalTraversalGrid& grid, size_t columns, size_t rows) {
    grid.columns = columns;
    grid.rows = rows;
    grid.values.assign(columns * rows, 0.f);
}

inline void clearOutput(AudioProcessContext& context) {
    auto output = makeOutputPayload(context, 0);
    payloadBuffer(output, context.frameCount).zero();
    publishSingleOutput(context, std::move(output));
}

inline SignalPayload* inputAt(AudioProcessContext& context, size_t index) {
    if (index >= context.inputs.size()) {
        return nullptr;
    }

    const size_t sampleCount = context.inputs[index].block.samples.size();
    if (sampleCount != 1 && sampleCount < context.frameCount) {
        return nullptr;
    }

    return &context.inputs[index];
}

inline float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
}

inline void publishVectorAsTraversalGrid(SignalPayload& payload, size_t columns) {
    if (payload.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    configureTraversalGrid(payload.traversalGrid, columns, payload.block.samples.size());
    const int rows = (int) payload.block.samples.size();

    for (size_t column = 0; column < columns; ++column) {
        Buffer<float>(
                payload.block.samples.data(),
                rows).copyTo({
                        payload.traversalGrid.values.data() + column * payload.block.samples.size(),
                        rows
                });
    }
}

}
