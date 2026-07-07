#pragma once

#include "../Graph/NodeGraph.h"

#include <vector>

namespace CycleV2 {

struct SignalBlock {
    std::vector<float> samples;
};

using AudioProcessBlock = SignalBlock;

struct SignalTraversalGrid {
    std::vector<float> values;
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

struct AudioProcessTiming {
    double sampleRate { 44100.0 };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
};

struct AudioProcessContext {
    size_t frameCount {};
    AudioProcessTiming timing;
    std::vector<NodeParameter> parameters;
    std::vector<SignalPayload> inputs;
    std::vector<AudioOutputPort> outputPorts;
    std::vector<SignalPayload> outputs;
};

}
