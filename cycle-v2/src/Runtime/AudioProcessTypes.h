#pragma once

#include "../Graph/NodeGraph.h"

#include <array>
#include <vector>

namespace CycleV2 {

struct PublishedNodeConfiguration;

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

enum class ControlEventKind {
    Controller,
    ChannelPressure
};

struct TimedControlEvent {
    ControlEventKind kind { ControlEventKind::Controller };
    size_t sampleOffset {};
    int controller {};
    float value {};
};

struct AudioVoiceControls {
    int noteNumber { 60 };
    int lowestNote { 0 };
    int highestNote { 127 };
    float velocity { 1.f };
    float normalizedVoiceTime {};
    float channelPressure {};
    std::array<float, 128> controllers {};
};

struct AudioVoiceContext {
    int voiceIndex {};
    std::vector<NoteLifecycleEvent> events;
    AudioVoiceControls controls;
    std::vector<TimedControlEvent> controlEvents;
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
    std::vector<SignalPayload*> inputViews;
    std::vector<SignalPayload> inputs;
    std::vector<AudioProcessAttachment> attachments;
    std::vector<AudioOutputPort> outputPorts;
    std::vector<SignalPayload*> outputViews;
    std::vector<SignalPayload> outputs;
};

}
