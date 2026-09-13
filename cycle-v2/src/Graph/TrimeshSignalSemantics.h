#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class TrimeshSignalSemantics {
public:
    static PortDomain domain(const Node& node);
    static bool isBipolar(const Node& node);
    static bool isBipolar(const std::vector<NodeParameter>& parameters);
};

}
