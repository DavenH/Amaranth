#pragma once

#include "NodeModuleRegistry.h"
#include "../Graph/NodeGraph.h"

#include <memory>
#include <vector>

namespace CycleV2 {

struct AudioProcessBlock {
    std::vector<float> samples;
    PortDomain domain { PortDomain::ControlSignal };
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct AudioOutputPort {
    String portId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct AudioProcessContext {
    size_t frameCount {};
    std::vector<NodeParameter> parameters;
    std::vector<AudioProcessBlock> inputs;
    std::vector<AudioOutputPort> outputPorts;
    AudioProcessBlock output;
    std::vector<AudioProcessBlock> outputs;
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
