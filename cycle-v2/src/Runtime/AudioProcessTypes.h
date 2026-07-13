#pragma once

#include "../Graph/NodeGraph.h"

#include <vector>

namespace CycleV2 {

struct SignalBlock {
    std::vector<float> samples;
};

using AudioProcessBlock = SignalBlock;

enum class TraversalGridArity {
    Empty,
    Scalar,
    Vector,
    Matrix
};

enum class TraversalGridAxis {
    None,
    Index,
    Time,
    Phase,
    Frequency,
    Morph,
    ImageX,
    ImageY,
    Repeated
};

enum class TraversalGridLayout {
    ColumnsThenRows
};

struct TraversalGridAxisResolution {
    double origin {};
    double step { 1.0 };
    String unit;
};

struct TraversalGridMetadata {
    TraversalGridArity arity { TraversalGridArity::Empty };
    TraversalGridLayout layout { TraversalGridLayout::ColumnsThenRows };
    TraversalGridAxis columnAxis { TraversalGridAxis::None };
    TraversalGridAxis rowAxis { TraversalGridAxis::None };
    PortDomain valueDomain { PortDomain::ControlSignal };
    TraversalGridAxisResolution columnResolution;
    TraversalGridAxisResolution rowResolution;
};

struct SignalTraversalGrid {
    std::vector<float> values;
    TraversalGridMetadata metadata;
    size_t columns {};
    size_t rows {};

    bool isValid() const {
        return columns > 0 && rows > 0 && values.size() >= columns * rows;
    }
};

struct SignalPayload {
    PortDomain domain { PortDomain::ControlSignal };
    ChannelLayout channelLayout { ChannelLayout::Mono };
    SignalBlock block;
    SignalTraversalGrid traversalGrid;
};

struct AudioOutputPort {
    String portId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct AudioProcessAttachment {
    String sourceNodeId;
    String sourcePortId;
    String destPortId;
    PortDomain domain {};
    SignalPayload payload;
};

struct AudioProcessTiming {
    double sampleRate { 44100.0 };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
};

struct AudioProcessWorkArena {
    size_t frameCapacity {};
    size_t gridValueCapacity {};
    size_t inputCapacity {};
    size_t outputCapacity {};

    void prepare(
            size_t frames,
            size_t maxInputs,
            size_t maxOutputs,
            size_t maxGridValues) {
        frameCapacity = frames;
        inputCapacity = maxInputs;
        outputCapacity = maxOutputs;
        gridValueCapacity = maxGridValues;
    }

    void reserve(SignalPayload& payload) const {
        payload.block.samples.reserve(frameCapacity);
    }

    void reserve(SignalTraversalGrid& grid) const {
        grid.values.reserve(gridValueCapacity);
    }

    void reserve(std::vector<SignalPayload>& payloads) const {
        payloads.reserve(outputCapacity);
    }
};

struct AudioProcessContext {
    size_t frameCount {};
    AudioProcessTiming timing;
    AudioProcessWorkArena* workArena {};
    std::vector<NodeParameter> parameters;
    std::vector<SignalPayload> inputs;
    std::vector<AudioProcessAttachment> attachments;
    std::vector<AudioOutputPort> outputPorts;
    std::vector<SignalPayload> outputs;
};

}
