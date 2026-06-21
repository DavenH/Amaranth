#pragma once

#include "NodeModuleRegistry.h"
#include "../Graph/NodeGraph.h"

#include <memory>
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

struct AudioProcessContext {
    size_t frameCount {};
    std::vector<NodeParameter> parameters;
    std::vector<SignalPayload> inputs;
    std::vector<AudioOutputPort> outputPorts;
    std::vector<SignalPayload> outputs;
};

class NodeAudioProcessor {
public:
    virtual ~NodeAudioProcessor() = default;

    virtual AudioModuleRole role() const = 0;
    virtual void prepare(size_t frameCount);
    virtual void process(AudioProcessContext& context) = 0;
};

class NodeAudioProcessorFactory {
public:
    std::unique_ptr<NodeAudioProcessor> create(AudioModuleRole role) const;
};

}
