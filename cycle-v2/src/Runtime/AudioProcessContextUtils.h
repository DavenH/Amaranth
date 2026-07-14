#pragma once

#include "AudioProcessTypes.h"
#include "../Graph/NodeDefinition.h"

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

inline TraversalGridArity traversalGridArity(size_t columns, size_t rows) {
    if (columns == 0 || rows == 0) {
        return TraversalGridArity::Empty;
    }

    if (columns == 1 && rows == 1) {
        return TraversalGridArity::Scalar;
    }

    if (columns == 1) {
        return TraversalGridArity::Vector;
    }

    return TraversalGridArity::Matrix;
}

inline TraversalGridAxis defaultTraversalRowAxisForDomain(PortDomain domain) {
    switch (domain) {
        case PortDomain::TimeSignal:
        case PortDomain::EnvelopeSignal:
            return TraversalGridAxis::Time;

        case PortDomain::SpectralMagnitudeSignal:
        case PortDomain::SpectralPhaseSignal:
            return TraversalGridAxis::Frequency;

        case PortDomain::ControlSignal:
        case PortDomain::DomainContext:
        default:
            return TraversalGridAxis::Index;
    }
}

inline TraversalGridMetadata makeTraversalGridMetadata(
        PortDomain valueDomain,
        size_t columns,
        size_t rows,
        TraversalGridAxis columnAxis,
        TraversalGridAxis rowAxis) {
    TraversalGridMetadata metadata;
    metadata.arity = traversalGridArity(columns, rows);
    metadata.columnAxis = columnAxis;
    metadata.rowAxis = rowAxis;
    metadata.valueDomain = valueDomain;
    return metadata;
}

inline bool traversalGridMetadataCompatible(
        const TraversalGridMetadata& left,
        const TraversalGridMetadata& right) {
    if (left.layout != right.layout) {
        return false;
    }

    if (left.valueDomain == PortDomain::ControlSignal
            || right.valueDomain == PortDomain::ControlSignal) {
        return true;
    }

    return left.valueDomain == right.valueDomain
            && left.rowAxis == right.rowAxis;
}

inline SignalPayload makeOutputPayload(const AudioOutputPort& port, size_t frameCount) {
    SignalPayload payload;
    payload.domain = port.domain;
    payload.channelLayout = port.channelLayout;
    payload.block.samples.resize(frameCount);
    return payload;
}

inline SignalPayload makeOutputPayload(AudioProcessContext& context, size_t index) {
    SignalPayload payload;
    if (index < context.outputPorts.size()) {
        payload = makeOutputPayload(context.outputPorts[index], context.frameCount);
    } else {
        payload.block.samples.resize(context.frameCount);
    }

    if (context.workArena != nullptr) {
        context.workArena->reserve(payload);
    }
    return payload;
}

inline void publishSingleOutput(AudioProcessContext& context, SignalPayload output) {
    context.outputs.clear();
    if (context.workArena != nullptr) {
        context.outputs.reserve(context.workArena->outputCapacity);
    }
    context.outputs.push_back(std::move(output));
}

inline void publishOutputs(AudioProcessContext& context, std::vector<SignalPayload> outputs) {
    context.outputs = std::move(outputs);
}

inline void publishOutputs(AudioProcessContext& context, SignalPayload first, SignalPayload second) {
    context.outputs.clear();
    if (context.workArena != nullptr) {
        context.outputs.reserve(context.workArena->outputCapacity);
    }
    context.outputs.push_back(std::move(first));
    context.outputs.push_back(std::move(second));
}

inline void configureTraversalGrid(
        SignalTraversalGrid& grid,
        size_t columns,
        size_t rows,
        TraversalGridMetadata metadata = {},
        const AudioProcessWorkArena* arena = nullptr) {
    grid.columns = columns;
    grid.rows = rows;
    metadata.arity = traversalGridArity(columns, rows);
    grid.metadata = metadata;
    if (arena != nullptr) {
        arena->reserve(grid);
    }
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
    if (context.inputs[index].traversalGrid.isValid()) {
        return &context.inputs[index];
    }

    if (sampleCount != 1 && sampleCount < context.frameCount) {
        return nullptr;
    }

    return &context.inputs[index];
}

inline float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    return typedParameterFloat(parameters, id, fallback);
}

inline void publishVectorAsTraversalGrid(
        SignalPayload& payload,
        size_t columns,
        const AudioProcessWorkArena* arena = nullptr) {
    if (payload.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    configureTraversalGrid(
            payload.traversalGrid,
            columns,
            payload.block.samples.size(),
            makeTraversalGridMetadata(
                    payload.domain,
                    columns,
                    payload.block.samples.size(),
                    TraversalGridAxis::Repeated,
                    defaultTraversalRowAxisForDomain(payload.domain)),
            arena);
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
