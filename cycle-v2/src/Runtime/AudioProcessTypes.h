#pragma once

#include "../Graph/NodeGraph.h"
#include "PreparedVector.h"
#include "SignalBuffer.h"

#include <limits>
#include <vector>

#include <Array/ScopedAlloc.h>

namespace CycleV2 {

struct PublishedNodeConfiguration;

struct SignalBlock {
    SignalBuffer samples;
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
    SignalBuffer values;
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
    SignalBlock secondaryBlock;
    SignalTraversalGrid traversalGrid;
    SignalTraversalGrid secondaryTraversalGrid;

    bool isStereo() const {
        return channelLayout == ChannelLayout::StereoPair;
    }
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
    SignalPayload* payload {};
};

struct AudioProcessTiming {
    double sampleRate { 44100.0 };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
};

enum class NoteLifecycleType {
    NoteOn,
    NoteOff,
    Reset
};

struct NoteLifecycleEvent {
    NoteLifecycleType type { NoteLifecycleType::NoteOn };
    size_t sampleOffset {};
    int voiceIndex {};
};

struct AudioVoiceContext {
    int voiceIndex {};
    std::vector<NoteLifecycleEvent> events;
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

    bool preparePayloadStorage(size_t payloadCount) {
        constexpr size_t maximumAllocation = (size_t) std::numeric_limits<int>::max();
        if (payloadCount > maximumAllocation / 2) {
            return false;
        }

        const size_t channelCount = payloadCount * 2;
        if ((frameCapacity != 0 && channelCount > maximumAllocation / frameCapacity)
                || (gridValueCapacity != 0
                        && channelCount > maximumAllocation / gridValueCapacity)) {
            return false;
        }

        const size_t blockValues = channelCount * frameCapacity;
        const size_t gridValues = channelCount * gridValueCapacity;

        blockMemory.resize((int) blockValues);
        gridMemory.resize((int) gridValues);
        blockMemory.resetPlacement();
        gridMemory.resetPlacement();
        return true;
    }

    void bind(SignalPayload& payload) {
        payload.block.samples.bind(blockMemory.place((int) frameCapacity));
        payload.secondaryBlock.samples.bind(blockMemory.place((int) frameCapacity));
        payload.traversalGrid.values.bind(gridMemory.place((int) gridValueCapacity));
        payload.secondaryTraversalGrid.values.bind(gridMemory.place((int) gridValueCapacity));
    }

    void reserve(SignalPayload& payload) const {
        payload.block.samples.reserve(frameCapacity);
        payload.traversalGrid.values.reserve(gridValueCapacity);
        if (payload.isStereo()) {
            payload.secondaryBlock.samples.reserve(frameCapacity);
            payload.secondaryTraversalGrid.values.reserve(gridValueCapacity);
        }
    }

    void reserve(SignalTraversalGrid& grid) const {
        grid.values.reserve(gridValueCapacity);
    }

    void reserve(std::vector<SignalPayload>& payloads) const {
        payloads.reserve(outputCapacity);
    }

    ScopedAlloc<float> blockMemory;
    ScopedAlloc<float> gridMemory;
};

struct AudioProcessContext {
    size_t frameCount {};
    AudioProcessTiming timing;
    const AudioVoiceContext* voiceView {};
    AudioVoiceContext voice;
    AudioProcessWorkArena* workArena {};
    const PublishedNodeConfiguration* configuration {};
    bool captureTraversalGrid { true };
    const std::vector<NodeParameter>* parameterView {};
    std::vector<NodeParameter> parameters;
    PreparedVector<SignalPayload*> inputViews;
    PreparedVector<SignalPayload> inputs;
    PreparedVector<AudioProcessAttachment> attachments;
    PreparedVector<AudioOutputPort> outputPorts;
    PreparedVector<SignalPayload*> outputViews;
    PreparedVector<SignalPayload> outputs;
};

}
