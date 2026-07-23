#pragma once

#include "../Graph/GraphEditor.h"

#include <optional>

namespace CycleV2 {

struct ModulationCableBundleRoute {
    PortAddress source;
    PortAddress destination;
};

struct ModulationCableBundle {
    static String portId();
    static PortAddress sourceAddress(const Node& node);
    static PortAddress destinationAddress(const Node& node);
    static bool isAddress(const PortAddress& address);
    static bool isSource(const NodeGraph& graph, const PortAddress& address);
    static bool isDestination(const NodeGraph& graph, const PortAddress& address);
    static std::vector<ModulationCableBundleRoute> routes(
            const NodeGraph& graph,
            const PortAddress& first,
            const PortAddress& second);
    static bool canConnect(
            const NodeGraph& graph,
            const PortAddress& first,
            const PortAddress& second);
    static std::vector<int> edgeIndices(const NodeGraph& graph, int edgeIndex);
    static std::optional<std::vector<int>> bundleBeginningAt(
            const NodeGraph& graph,
            int edgeIndex);
    static Point<float> worldCentre(const Node& node, bool input);
};

}
