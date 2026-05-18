#pragma once

#include "NodeGraph.h"

namespace CycleV2 {

class GraphNodeFactory {
public:
    Node createNode(NodeKind kind, const String& id, Point<float> position) const;

private:
    Port input(
            String id,
            String label,
            PortDomain domain,
            ChannelLayout layout = ChannelLayout::Mono,
            PortPurpose purpose = PortPurpose::Signal) const;
    Port output(
            String id,
            String label,
            PortDomain domain,
            ChannelLayout layout = ChannelLayout::Mono) const;
};

}
