#pragma once

#include <optional>

#include "NodePortGeometry.h"
#include "../Graph/GraphEditor.h"

namespace CycleV2 {

struct ModulationCableBundleRoute {
    PortAddress source;
    PortAddress destination;
};

struct ModulationCableBundle {
    static constexpr float socketDiameter = NodePortGeometry::socketDiameter;

    static String portId();
    static PortAddress sourceAddress(const Node& node);
    static PortAddress destinationAddress(const Node& node);
    static bool isAddress(const PortAddress& address);
    static bool isSource(const NodeGraph& graph, const PortAddress& address);
    static bool isDestination(const NodeGraph& graph, const PortAddress& address);
    static bool supportsDestination(const Node& node);
    static bool destinationIncludesYellow(const Node& node);
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
    static bool hidesIndividualPort(const Node& node, const Port& port);
    static bool usesSharedSourceSocket(const Node& node, const Edge& edge);
    static Point<float> worldCentre(const Node& node, bool input);
};

}
