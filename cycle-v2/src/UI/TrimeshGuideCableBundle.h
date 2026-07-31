#pragma once

#include "../Graph/NodeGraph.h"

#include <optional>
#include <vector>

namespace CycleV2 {

struct TrimeshGuideCableBundle {
    static std::vector<int> edgeIndices(const NodeGraph& graph, int edgeIndex);
    static std::optional<std::vector<int>> bundleBeginningAt(
            const NodeGraph& graph,
            int edgeIndex);
};

}
