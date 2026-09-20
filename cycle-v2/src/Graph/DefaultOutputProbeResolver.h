#pragma once

#include <optional>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct DefaultOutputProbeAddress {
    String sourceNodeId;
    String sourcePortId;
};

class DefaultOutputProbeResolver {
public:
    static constexpr auto probeId = "default-output";

    std::optional<DefaultOutputProbeAddress> resolve(const NodeGraph& graph) const;
};

}
