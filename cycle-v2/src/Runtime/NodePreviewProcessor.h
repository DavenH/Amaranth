#pragma once

#include "NodeModuleRegistry.h"
#include "../Graph/NodeGraph.h"

#include <memory>
#include <vector>

namespace CycleV2 {

struct PreviewOutputPort {
    String portId;
    PortDomain domain {};
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

struct PreviewProcessContext {
    size_t pointCount {};
    std::vector<NodeParameter> parameters;
    std::vector<PreviewOutputPort> outputPorts;
    std::vector<float> inputSummary;
    std::vector<float> inputGrid;
    size_t inputGridColumns {};
    size_t inputGridRows {};
    std::vector<float> primary;
    std::vector<float> secondary;
    size_t gridColumns {};
    size_t gridRows {};
    PortDomain domain { PortDomain::TimeSignal };
};

class NodePreviewProcessor {
public:
    virtual ~NodePreviewProcessor() = default;

    virtual PreviewModuleRole role() const = 0;
    virtual void render(PreviewProcessContext& context) = 0;
};

class NodePreviewProcessorFactory {
public:
    std::unique_ptr<NodePreviewProcessor> create(PreviewModuleRole role) const;
};

}
