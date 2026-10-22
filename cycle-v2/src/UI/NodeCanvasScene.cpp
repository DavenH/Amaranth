#include <algorithm>
#include <limits>

#include "UI/NodeCanvasScene.h"
#include "Graph/MeshDimensionality.h"
#include "Graph/GraphEdgeIndex.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodeInputQuickCreate.h"
#include "UI/NodePortGeometry.h"
#include "UI/NodePortLayout.h"
#include "UI/NodeViewModule.h"
#include "UI/NodeGroupPresentation.h"

namespace CycleV2 {

namespace {

Rectangle<float> quickCreateHitBounds(
        Rectangle<float> bounds,
        Point<float> centre,
        float zoom) {
    const float rowHeight = NodePortGeometry::sidePortSpacing * zoom;
    return {
            bounds.getX() - 8.f,
            centre.y - rowHeight * 0.5f,
            bounds.getWidth() + 8.f,
            rowHeight
    };
}

int portIndexOnSide(const Node& node, const Port& port) {
    if (port.input && operationPortLayout(node) == OperationPortLayout::SideReversed) {
        if (port.id == node.inputs[0].id) {
            return 1;
        }
        if (port.id == node.inputs[1].id) {
            return 0;
        }
    }

    int index = 0;
    auto scan = [&](const std::vector<Port>& ports) {
        for (const auto& candidate : ports) {
            if (candidate.side != port.side) {
                continue;
            }
            if (candidate.id == port.id && candidate.input == port.input) {
                return true;
            }
            ++index;
        }
        return false;
    };

    return scan(node.inputs) || scan(node.outputs) ? index : 0;
}

int portCountOnSide(const Node& node, PortSide side) {
    return (int) std::count_if(node.inputs.begin(), node.inputs.end(), [&](const auto& port) {
        return port.side == side;
    }) + (int) std::count_if(node.outputs.begin(), node.outputs.end(), [&](const auto& port) {
        return port.side == side;
    });
}

const Node* findNode(const NodeGraph& graph, const juce::String& nodeId) {
    return graph.findNode(nodeId);
}

const Port* findPort(const Node& node, const juce::String& portId, bool input) {
    const auto& ports = input ? node.inputs : node.outputs;
    const auto found = std::find_if(ports.begin(), ports.end(), [&](const auto& port) {
        return port.id == portId;
    });
    return found == ports.end() ? nullptr : &*found;
}

const Edge* singleSignalEdgeForNode(
        const NodeGraph& graph,
        const GraphEdgeIndex& edgeIndex,
        const juce::String& nodeId,
        bool incoming) {
    const Edge* result = nullptr;
    const auto& candidates = incoming
            ? edgeIndex.incomingEdges(nodeId)
            : edgeIndex.outgoingEdges(nodeId);
    for (const size_t candidate : candidates) {
        const Edge& edge = graph.getEdges()[candidate];
        if (edge.connectionKind != ConnectionKind::Signal) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &edge;
    }
    return result;
}

struct InlinePanRoute {
    const Edge* incoming {};
    const Edge* outgoing {};
    const Node* sourceNode {};
    const Node* destinationNode {};
    const Port* sourcePort {};
    const Port* destinationPort {};
    int incomingIndex { -1 };
    int outgoingIndex { -1 };
};

std::optional<InlinePanRoute> inlinePanRoute(
        const NodeGraph& graph,
        const GraphEdgeIndex& edgeIndex,
        const Node& node) {
    const Edge* incoming = singleSignalEdgeForNode(graph, edgeIndex, node.id, true);
    const Edge* outgoing = singleSignalEdgeForNode(graph, edgeIndex, node.id, false);
    if (incoming == nullptr || outgoing == nullptr) {
        return std::nullopt;
    }

    const Node* sourceNode = findNode(graph, incoming->sourceNodeId);
    const Node* destinationNode = findNode(graph, outgoing->destNodeId);
    if (sourceNode == nullptr || destinationNode == nullptr) {
        return std::nullopt;
    }

    const Port* sourcePort = findPort(*sourceNode, incoming->sourcePortId, false);
    const Port* destinationPort = findPort(*destinationNode, outgoing->destPortId, true);
    if (sourcePort == nullptr || destinationPort == nullptr) {
        return std::nullopt;
    }

    const Edge* edgeData = graph.getEdges().data();
    return InlinePanRoute {
            incoming,
            outgoing,
            sourceNode,
            destinationNode,
            sourcePort,
            destinationPort,
            (int) std::distance(edgeData, incoming),
            (int) std::distance(edgeData, outgoing)
    };
}

juce::Point<float> outwardNormal(PortSide side) {
    switch (side) {
        case PortSide::Top:    return { 0.f, -1.f };
        case PortSide::Bottom: return { 0.f, 1.f };
        case PortSide::Right:  return { 1.f, 0.f };
        case PortSide::Left:
        default:               return { -1.f, 0.f };
    }
}

juce::Point<float> normalizedOrFallback(
        juce::Point<float> vector,
        juce::Point<float> fallback) {
    const float length = vector.getDistanceFromOrigin();
    return length > 0.0001f ? vector / length : fallback;
}

juce::Path buildCablePath(
        juce::Point<float> source,
        juce::Point<float> destination,
        PortSide sourceSide,
        PortSide destinationSide,
        float zoom) {
    juce::Path result;
    result.startNewSubPath(source);
    const auto vector = destination - source;
    const float distance = vector.getDistanceFromOrigin();
    if (std::abs(vector.x) <= 0.5f || std::abs(vector.y) <= 0.5f) {
        result.lineTo(destination);
        return result;
    }

    const float sourceStrength = juce::jlimit(24.f * zoom, 120.f * zoom, distance * 0.34f);
    const float destinationStrength = juce::jlimit(18.f * zoom, 74.f * zoom, distance * 0.18f);
    result.cubicTo(
            source + normalizedOrFallback(vector, outwardNormal(sourceSide)) * sourceStrength,
            destination + outwardNormal(destinationSide) * destinationStrength,
            destination);
    return result;
}

float inlinePanFraction(const NodeGraph& graph, const InlinePanRoute& route) {
    const bool hasProbeBefore = graph.findSignalProbeForSource(
            route.incoming->sourceNodeId,
            route.incoming->sourcePortId) != nullptr;
    const bool hasProbeAfter = graph.findSignalProbeForSource(
            route.outgoing->sourceNodeId,
            route.outgoing->sourcePortId) != nullptr;
    return hasProbeBefore ? 2.f / 3.f : (hasProbeAfter ? 1.f / 3.f : 0.5f);
}

Path inlinePanPath(const InlinePanRoute& route) {
    const Point<float> source = NodeCanvasScene::portWorldCentre(
            *route.sourceNode,
            *route.sourcePort);
    const Point<float> destination = NodeCanvasScene::portWorldCentre(
            *route.destinationNode,
            *route.destinationPort);
    return buildCablePath(
            source,
            destination,
            route.sourcePort->side,
            route.destinationPort->side,
            1.f);
}

bool crossesNodeBody(
        const Path& path,
        const NodeCanvasSceneSnapshot& scene,
        const juce::String& sourceNodeId,
        const juce::String& destinationNodeId) {
    const Rectangle<float> pathBounds = path.getBounds();
    for (const auto& target : scene.targets) {
        if (target.kind != NodeSceneTargetKind::Node
                || target.nodeId == sourceNodeId
                || target.nodeId == destinationNodeId
                || !target.bounds.intersects(pathBounds)) {
            continue;
        }

        const Rectangle<float> body = target.bounds.reduced(3.f);
        const float length = path.getLength();
        for (int sample = 1; sample < 32; ++sample) {
            if (body.contains(path.getPointAlongPath(length * (float) sample / 32.f))) {
                return true;
            }
        }
    }
    return false;
}

int fanOutLane(
        const NodeGraph& graph,
        const GraphEdgeIndex& edgeIndex,
        const Edge& edge,
        const Node& sourceNode,
        const Node& destinationNode) {
    int lane = 0;
    const auto& graphEdges = graph.getEdges();
    for (const size_t candidateIndex : edgeIndex.outgoingEdges(edge.sourceNodeId)) {
        const Edge& candidate = graphEdges[candidateIndex];
        if (candidate.sourcePortId != edge.sourcePortId
                || candidate.connectionKind != edge.connectionKind) {
            continue;
        }
        const Node* siblingDestination = graph.findNode(candidate.destNodeId);
        if (siblingDestination == nullptr
                || siblingDestination->bounds.getX() <= sourceNode.bounds.getRight()) {
            continue;
        }
        if (siblingDestination->bounds.getX() < destinationNode.bounds.getX()) {
            ++lane;
        }
    }
    return lane;
}

float corridorTop(
        const NodeCanvasSceneSnapshot& scene,
        const juce::String& sourceNodeId,
        float leadX,
        float destinationX,
        float destinationTop) {
    float top = destinationTop;
    for (const auto& target : scene.targets) {
        if (target.kind == NodeSceneTargetKind::Node
                && target.nodeId != sourceNodeId
                && target.bounds.getX() < destinationX
                && target.bounds.getRight() > leadX) {
            top = std::min(top, target.bounds.getY());
        }
    }
    return top;
}

Path cableDetourPath(
        const NodeSceneEdge& presented,
        float laneY,
        float zoom) {
    const float span = presented.destination.x - presented.source.x;
    Path routed;
    routed.startNewSubPath(presented.source);
    routed.cubicTo(
            { presented.source.x + span * 0.42f,
                    std::min(presented.source.y, laneY) - 16.f * zoom },
            { presented.destination.x - 30.f * zoom, laneY },
            presented.destination);
    return routed;
}

void routeObstructedCables(
        const NodeGraph& graph,
        const GraphEdgeIndex& edgeIndex,
        const NodeCanvasViewport& viewport,
        NodeCanvasSceneSnapshot& scene) {
    const float zoom = viewport.getZoom();
    const auto& graphEdges = graph.getEdges();
    for (auto& presented : scene.edges) {
        if (presented.inlinePan || !isPositiveAndBelow(
                    presented.edgeIndex, (int) graphEdges.size())) {
            continue;
        }

        const Edge& edge = graphEdges[(size_t) presented.edgeIndex];
        const Node* sourceNode = graph.findNode(edge.sourceNodeId);
        const Node* destinationNode = graph.findNode(edge.destNodeId);
        if (sourceNode == nullptr || destinationNode == nullptr) {
            continue;
        }
        const Port* sourcePort = findPort(*sourceNode, edge.sourcePortId, false);
        if (sourcePort == nullptr || sourcePort->side != PortSide::Right) {
            continue;
        }

        const Rectangle<float> destinationBounds = viewport.toScreen(destinationNode->bounds);
        const bool entersLeft = std::abs(presented.destination.x - destinationBounds.getX())
                <= 4.f * zoom;
        if (!entersLeft || !crossesNodeBody(
                    presented.cablePath, scene, edge.sourceNodeId, edge.destNodeId)) {
            continue;
        }

        if (presented.destination.x <= presented.source.x + 72.f * zoom) {
            continue;
        }
        const int lane = fanOutLane(
                graph, edgeIndex, edge, *sourceNode, *destinationNode);
        const float rowTop = corridorTop(
                scene, edge.sourceNodeId, presented.source.x, presented.destination.x,
                destinationBounds.getY());
        Path routed;
        for (int attempt = 0; attempt < 3; ++attempt) {
            const float laneY = rowTop
                    - (30.f + (float) lane * 18.f + (float) attempt * 30.f) * zoom;
            routed = cableDetourPath(presented, laneY, zoom);
            if (!crossesNodeBody(routed, scene, edge.sourceNodeId, edge.destNodeId)) {
                break;
            }
        }
        if (crossesNodeBody(routed, scene, edge.sourceNodeId, edge.destNodeId)) {
            continue;
        }
        presented.cablePath = std::move(routed);
        presented.hitPath.clear();
        PathStrokeType(22.f, PathStrokeType::curved, PathStrokeType::rounded)
                .createStrokedPath(presented.hitPath, presented.cablePath);
    }
}

std::optional<Point<float>> inlinePanCentre(
        const NodeGraph& graph,
        const GraphEdgeIndex& edgeIndex,
        const Node& node) {
    const std::optional<InlinePanRoute> route = inlinePanRoute(graph, edgeIndex, node);
    if (!route.has_value()) {
        return std::nullopt;
    }

    const Path path = inlinePanPath(*route);
    return path.getPointAlongPath(path.getLength() * inlinePanFraction(graph, *route));
}

NodeSceneEdge inlinePanSceneEdge(
        const InlinePanRoute& route,
        const NodeCanvasViewport& viewport) {
    const Point<float> source = viewport.toScreen(NodeCanvasScene::portWorldCentre(
            *route.sourceNode,
            *route.sourcePort));
    const Point<float> destination = viewport.toScreen(NodeCanvasScene::portWorldCentre(
            *route.destinationNode,
            *route.destinationPort));
    Path visiblePath = buildCablePath(
            source,
            destination,
            route.sourcePort->side,
            route.destinationPort->side,
            viewport.getZoom());
    Path hitPath;
    PathStrokeType(22.f, PathStrokeType::curved, PathStrokeType::rounded)
            .createStrokedPath(hitPath, visiblePath);

    NodeSceneEdge result;
    result.edgeIndex = route.incomingIndex;
    result.edgeIndices = { route.incomingIndex, route.outgoingIndex };
    result.source = source;
    result.destination = destination;
    result.cablePath = std::move(visiblePath);
    result.hitPath = std::move(hitPath);
    result.inlinePan = true;
    return result;
}

}

juce::Point<float> NodeCanvasScene::portWorldCentre(const Node& node, const Port& port) {
    if (node.kind == NodeKind::ModulationSource || node.kind == NodeKind::SpectralLayer) {
        return {
                port.input ? node.bounds.getX() : node.bounds.getRight(),
                node.bounds.getCentreY()
        };
    }

    if (port.side == PortSide::Top || port.side == PortSide::Bottom) {
        const int index = portIndexOnSide(node, port);
        const int count = juce::jmax(1, portCountOnSide(node, port.side));
        return {
                node.bounds.getX() + node.bounds.getWidth()
                        * ((float) index + 1.f) / ((float) count + 1.f),
                port.side == PortSide::Top ? node.bounds.getY() : node.bounds.getBottom()
        };
    }

    const float y = node.bounds.getY()
            + NodePortGeometry::firstSidePortOffset
            + (float) portIndexOnSide(node, port) * NodePortGeometry::sidePortSpacing;
    return {
            port.side == PortSide::Right ? node.bounds.getRight() : node.bounds.getX(),
            y
    };
}

juce::Rectangle<float> NodeCanvasScene::presentationWorldBounds(
        const NodeGraph& graph,
        const Node& node,
        const GraphEdgeIndex& edgeIndex) {
    if (node.kind != NodeKind::SpectralLayer) {
        return node.bounds;
    }

    const std::optional<Point<float>> centre = inlinePanCentre(graph, edgeIndex, node);
    return centre.has_value() ? node.bounds.withCentre(*centre) : node.bounds;
}

int NodeCanvasScene::cableExtraEdgeIndex(
        const NodeGraph& graph,
        int edgeIndex,
        const GraphEdgeIndex& graphEdgeIndex) {
    if (!isPositiveAndBelow(edgeIndex, (int) graph.getEdges().size())) {
        return edgeIndex;
    }

    const Edge& edge = graph.getEdges()[(size_t) edgeIndex];
    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    if (sourceNode == nullptr || sourceNode->kind != NodeKind::SpectralLayer) {
        return edgeIndex;
    }

    const Edge* incoming = singleSignalEdgeForNode(
            graph, graphEdgeIndex, sourceNode->id, true);
    return incoming != nullptr
            ? (int) std::distance(graph.getEdges().data(), incoming)
            : edgeIndex;
}

juce::Path NodeCanvasScene::cablePath(
        juce::Point<float> source,
        juce::Point<float> destination,
        PortSide sourceSide,
        PortSide destinationSide,
        float zoom) {
    return buildCablePath(source, destination, sourceSide, destinationSide, zoom);
}

const NodeCanvasSceneSnapshot& NodeCanvasScene::build(
        const NodeGraph& graph,
        const NodeCanvasViewport& viewport,
        uint64_t presentationRevision,
        uint64_t documentRevision,
        const GraphEdgeIndex* suppliedEdgeIndex) {
    const uint64_t graphRevision = graph.getRevision();
    if (current.graphRevision == graphRevision
            && current.documentRevision == documentRevision
            && current.viewportRevision == viewport.getRevision()
            && current.presentationRevision == presentationRevision) {
        return current;
    }

    current = {};
    current.graphRevision = graphRevision;
    current.documentRevision = documentRevision;
    current.viewportRevision = viewport.getRevision();
    current.presentationRevision = presentationRevision;

    const std::optional<GraphEdgeIndex> localEdgeIndex = suppliedEdgeIndex == nullptr
            ? std::optional<GraphEdgeIndex>(std::in_place, graph.getEdges())
            : std::nullopt;
    const GraphEdgeIndex& indexedEdges = suppliedEdgeIndex != nullptr
            ? *suppliedEdgeIndex
            : *localEdgeIndex;

    int zOrder = 100;
    for (const auto& group : graph.getNodeGroups()) {
        current.targets.push_back({
                NodeSceneTargetKind::NodeGroupToggle,
                "groupToggle:" + group.id,
                group.id,
                {},
                {},
                NodeGroupPresentation::toggleBounds(
                        graph, group, viewport, indexedEdges),
                -1,
                30000 + zOrder++
        });
        const auto groupBounds = viewport.toScreen(NodeGroupPresentation::worldBounds(
                graph, group, viewport, indexedEdges));
        const auto background = group.collapsed
                ? groupBounds.reduced(13.f).withTrimmedTop(29.f)
                : groupBounds.withTrimmedTop(groupBounds.getHeight() - 15.f)
                        .reduced(10.f, 0.f);
        current.targets.push_back({
                NodeSceneTargetKind::NodeGroupBackground,
                "groupBackground:" + group.id,
                group.id,
                {},
                {},
                background,
                -1,
                30000 + zOrder++
        });
    }
    for (const auto& node : graph.getNodes()) {
        if (!graph.isNodeVisible(node.id)) {
            continue;
        }
        const std::optional<Point<float>> inlineCentre = node.kind == NodeKind::SpectralLayer
                ? inlinePanCentre(graph, indexedEdges, node)
                : std::nullopt;
        const Rectangle<float> presentationBounds = inlineCentre.has_value()
                ? node.bounds.withCentre(*inlineCentre)
                : node.bounds;
        current.targets.push_back({
                NodeSceneTargetKind::Node,
                "node:" + node.id,
                node.id,
                {},
                {},
                viewport.toScreen(presentationBounds),
                -1,
                zOrder++
        });

        auto appendPorts = [&](const std::vector<Port>& ports,
                NodeSceneTargetKind kind,
                bool configurationOnly = false) {
            if (node.kind == NodeKind::SpectralLayer) {
                return;
            }
            for (const auto& port : ports) {
                if ((port.input && !MeshDimensionality::inputActive(node, port.id)
                            && indexedEdges.edgesToInput(node.id, port.id).empty())
                        || ModulationCableBundle::hidesIndividualPort(node, port)
                        || (configurationOnly
                            && port.connectionKind != ConnectionKind::ConfigurationAttachment)) {
                    continue;
                }
                const auto centre = viewport.toScreen(portWorldCentre(node, port));
                const float size = NodePortGeometry::socketDiameter
                        * viewport.getZoom()
                        / NodePortGeometry::referenceZoom;
                const float hitPadding = node.kind == NodeKind::SpectralLayer
                        ? 4.f
                        : NodePortGeometry::hitPadding;
                Rectangle<float> hitBounds = Rectangle<float>(size, size)
                        .withCentre(centre)
                        .expanded(hitPadding);
                if (port.input && NodeInputQuickCreate::sourceFor(
                            node, { node.id, port.id, true }).has_value()) {
                    hitBounds = quickCreateHitBounds(
                            hitBounds, centre, viewport.getZoom());
                }
                current.targets.push_back({
                        kind,
                        (port.input ? "input:" : "output:") + node.id + "." + port.id,
                        node.id,
                        port.id,
                        {},
                        hitBounds,
                        -1,
                        10000 + zOrder++
                });
            }
        };
        appendPorts(node.inputs, NodeSceneTargetKind::InputPort);
        if (node.kind != NodeKind::ModulationTriple) {
            appendPorts(node.outputs, NodeSceneTargetKind::OutputPort);
        }
        if (node.kind == NodeKind::ModulationTriple
                || ModulationCableBundle::socketVisible(node, indexedEdges)) {
            const bool input = node.kind != NodeKind::ModulationTriple;
            const auto centre = viewport.toScreen(
                    ModulationCableBundle::worldCentre(node, input));
            const float size = ModulationCableBundle::socketDiameter
                    * viewport.getZoom() / NodePortGeometry::referenceZoom;
            Rectangle<float> hitBounds = Rectangle<float>(size, size)
                    .withCentre(centre).expanded(NodePortGeometry::hitPadding);
            if (input && NodeInputQuickCreate::sourceFor(
                        node, ModulationCableBundle::destinationAddress(node)).has_value()) {
                hitBounds = quickCreateHitBounds(
                        hitBounds, centre, viewport.getZoom());
            }
            current.targets.push_back({
                    input ? NodeSceneTargetKind::InputPort : NodeSceneTargetKind::OutputPort,
                    (input ? "input:" : "output:") + node.id + ".modulationBundle",
                    node.id,
                    ModulationCableBundle::portId(),
                    {},
                    hitBounds,
                    -1,
                    20000 + zOrder++
            });
        }
    }

    for (int edgeIndex = 0; edgeIndex < (int) graph.getEdges().size(); ++edgeIndex) {
        const auto& edge = graph.getEdges()[(size_t) edgeIndex];
        const auto modulationBundle = ModulationCableBundle::bundleBeginningAt(graph, edgeIndex);
        const auto modulationIndices = ModulationCableBundle::edgeIndices(graph, edgeIndex);
        const auto bundleIndices = modulationIndices;
        if (modulationIndices.size() > 1 && !modulationBundle.has_value()) {
            continue;
        }
        const Node* sourceNode = findNode(graph, edge.sourceNodeId);
        const Node* destinationNode = findNode(graph, edge.destNodeId);
        if (sourceNode == nullptr || destinationNode == nullptr) {
            continue;
        }
        const NodeGroup* sourceGroup = graph.groupForNode(edge.sourceNodeId);
        const NodeGroup* destinationGroup = graph.groupForNode(edge.destNodeId);
        const bool sourceCollapsed = !graph.isNodeVisible(edge.sourceNodeId);
        const bool destinationCollapsed = !graph.isNodeVisible(edge.destNodeId);
        if (sourceCollapsed && sourceGroup == destinationGroup) {
            continue;
        }

        if (!sourceCollapsed && !destinationCollapsed
                && sourceNode->kind == NodeKind::SpectralLayer) {
            const std::optional<InlinePanRoute> route = inlinePanRoute(
                    graph, indexedEdges, *sourceNode);
            if (route.has_value() && route->outgoingIndex == edgeIndex) {
                continue;
            }
        }
        if (!sourceCollapsed && !destinationCollapsed
                && destinationNode->kind == NodeKind::SpectralLayer) {
            const std::optional<InlinePanRoute> route = inlinePanRoute(
                    graph, indexedEdges, *destinationNode);
            if (route.has_value() && route->incomingIndex == edgeIndex) {
                current.edges.push_back(inlinePanSceneEdge(*route, viewport));
                continue;
            }
        }

        const Port* sourcePort = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* destinationPort = findPort(*destinationNode, edge.destPortId, true);
        if (sourcePort == nullptr) {
            continue;
        }

        const bool isModulationBundle = modulationBundle.has_value();
        const bool usesSharedModulationSource = isModulationBundle
                || ModulationCableBundle::usesSharedSourceSocket(*sourceNode, edge);
        Node presentedSourceNode = *sourceNode;
        const auto sourceCentre = sourceNode->kind == NodeKind::SpectralLayer
                ? inlinePanCentre(graph, indexedEdges, *sourceNode)
                : std::nullopt;
        presentedSourceNode.bounds = sourceCentre.has_value()
                ? sourceNode->bounds.withCentre(*sourceCentre)
                : sourceNode->bounds;
        Node presentedDestinationNode = *destinationNode;
        const auto destinationCentre = destinationNode->kind == NodeKind::SpectralLayer
                ? inlinePanCentre(graph, indexedEdges, *destinationNode)
                : std::nullopt;
        presentedDestinationNode.bounds = destinationCentre.has_value()
                ? destinationNode->bounds.withCentre(*destinationCentre)
                : destinationNode->bounds;
        auto source = viewport.toScreen(usesSharedModulationSource
                ? ModulationCableBundle::worldCentre(presentedSourceNode, false)
                : portWorldCentre(presentedSourceNode, *sourcePort));
        const auto attachmentCentre = NodeViewModuleRegistry::instance()
                .moduleFor(destinationNode->kind).attachmentWorldCentre(*destinationNode, edge.destPortId);
        if (destinationPort == nullptr && !attachmentCentre.has_value()) {
            continue;
        }
        auto destination = viewport.toScreen(isModulationBundle
                ? ModulationCableBundle::worldCentre(presentedDestinationNode, true)
                : destinationPort != nullptr
                ? portWorldCentre(presentedDestinationNode, *destinationPort)
                : *attachmentCentre);
        PortSide sourceSide = usesSharedModulationSource ? PortSide::Right : sourcePort->side;
        PortSide destinationSide = destinationPort != nullptr ? destinationPort->side : PortSide::Top;
        if (sourceCollapsed) {
            const auto world = NodeGroupPresentation::boundaryPoint(
                    graph, *sourceGroup, viewport.toWorld(destination), viewport, indexedEdges);
            source = viewport.toScreen(world);
            sourceSide = source.x <= destination.x ? PortSide::Right : PortSide::Left;
        }
        if (destinationCollapsed) {
            const auto world = NodeGroupPresentation::boundaryPoint(
                    graph, *destinationGroup, viewport.toWorld(source), viewport, indexedEdges);
            destination = viewport.toScreen(world);
            destinationSide = destination.x >= source.x ? PortSide::Left : PortSide::Right;
        }
        juce::Path visiblePath = cablePath(
                source,
                destination,
                sourceSide,
                destinationSide,
                viewport.getZoom());
        juce::Path hitPath;
        juce::PathStrokeType(22.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath(hitPath, visiblePath);
        current.edges.push_back({
                edgeIndex,
                bundleIndices,
                source,
                destination,
                std::move(visiblePath),
                std::move(hitPath),
                destinationPort != nullptr,
                isModulationBundle,
                !isModulationBundle
                        || ModulationCableBundle::destinationIncludesYellow(*destinationNode),
                sourceNode->kind != NodeKind::SpectralLayer,
                destinationNode->kind != NodeKind::SpectralLayer,
                false
        });
    }

    for (auto& presented : current.edges) {
        if (!isPositiveAndBelow(presented.edgeIndex, (int) graph.getEdges().size())) {
            continue;
        }
        const Edge& edge = graph.getEdges()[(size_t) presented.edgeIndex];
        const Node* destination = graph.findNode(edge.destNodeId);
        if (destination == nullptr || !MeshDimensionality::applies(destination->kind)) {
            continue;
        }
        presented.destinationMorphMask = MeshDimensionality::dimensions(*destination).cornerMask();
        presented.inactive = !MeshDimensionality::inputActive(*destination, edge.destPortId);
        if (presented.modulationBundle) {
            presented.inactive = std::all_of(presented.edgeIndices.begin(), presented.edgeIndices.end(),
                    [&](int index) {
                        return !MeshDimensionality::inputActive(*destination,
                                graph.getEdges()[(size_t) index].destPortId);
                    });
        }
    }
    routeObstructedCables(graph, indexedEdges, viewport, current);
    return current;
}

std::optional<NodeSceneTarget> NodeCanvasHitTester::hitTest(
        const NodeCanvasSceneSnapshot& scene,
        juce::Point<float> screenPosition) const {
    const NodeSceneTarget* bestTarget = nullptr;
    for (const auto& target : scene.targets) {
        if (target.bounds.contains(screenPosition)
                && (bestTarget == nullptr || target.zOrder >= bestTarget->zOrder)) {
            bestTarget = &target;
        }
    }
    if (bestTarget != nullptr) {
        return *bestTarget;
    }

    const NodeSceneEdge* closestEdge = nullptr;
    float closestDistance = std::numeric_limits<float>::max();
    for (const auto& edge : scene.edges) {
        if (edge.hitPath.contains(screenPosition)) {
            Point<float> nearest;
            edge.cablePath.getNearestPoint(screenPosition, nearest);
            const float distance = nearest.getDistanceFrom(screenPosition);
            if (distance < closestDistance) {
                closestDistance = distance;
                closestEdge = &edge;
            }
        }
    }

    if (closestEdge != nullptr) {
        NodeSceneTarget target;
        target.kind = NodeSceneTargetKind::Edge;
        target.semanticId = "edge:" + juce::String(closestEdge->edgeIndex);
        target.edgeIndex = closestEdge->edgeIndex;
        return target;
    }

    return std::nullopt;
}

}
