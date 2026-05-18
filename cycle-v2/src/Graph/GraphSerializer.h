#pragma once

#include "NodeGraph.h"

namespace CycleV2 {

class GraphSerializer {
public:
    ValueTree toValueTree(const NodeGraph& graph) const;
    NodeGraph fromValueTree(const ValueTree& tree) const;
};

}
