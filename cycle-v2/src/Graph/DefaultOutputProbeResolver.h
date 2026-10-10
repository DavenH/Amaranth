#pragma once

#include <optional>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct DefaultOutputProbeAddress {
    String sourceNodeId;
    String sourcePortId;
    String destNodeId;
    String destPortId;
};

class DefaultOutputProbeResolver {
public:
    static constexpr auto probeId = "default-output";

    std::optional<DefaultOutputProbeAddress> resolve(const NodeGraph& graph) const;
};

}
