#pragma once

#include "NodeGraph.h"

namespace CycleV2 {

class GraphSerializer {
public:
    ValueTree toValueTree(const NodeGraph& graph) const;
    NodeGraph fromValueTree(const ValueTree& tree) const;
    String toXmlString(const NodeGraph& graph) const;
    NodeGraph fromXmlString(const String& xml) const;
};

}
