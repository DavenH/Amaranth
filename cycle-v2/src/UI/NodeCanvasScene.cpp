#include "UI/NodeCanvasScene.h"
#include "UI/ModulationCableBundle.h"
#include "UI/NodePortGeometry.h"
#include "UI/NodeViewModule.h"

#include <algorithm>

namespace CycleV2 {

namespace {

int portIndexOnSide(const Node& node, const Port& port) {
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
        const juce::String& nodeId,
        bool incoming) {
    const Edge* result = nullptr;
    for (const auto& edge : graph.getEdges()) {
        const bool matches = incoming
                ? edge.destNodeId == nodeId
                : edge.sourceNodeId == nodeId;
        if (!matches || edge.connectionKind != ConnectionKind::Signal) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &edge;
    }
    return result;
}

std::optional<Point<float>> inlinePanCentre(
        const NodeGraph& graph,
        const Node& node) {
    const Edge* incoming = singleSignalEdgeForNode(graph, node.id, true);
    const Edge* outgoing = singleSignalEdgeForNode(graph, node.id, false);
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

    const Point<float> source = NodeCanvasScene::portWorldCentre(*sourceNode, *sourcePort);
    const Point<float> destination = NodeCanvasScene::portWorldCentre(
            *destinationNode,
            *destinationPort);
    const bool hasProbeBefore = graph.findSignalProbeForSource(
            incoming->sourceNodeId,
            incoming->sourcePortId) != nullptr;
    const bool hasProbeAfter = graph.findSignalProbeForSource(
            outgoing->sourceNodeId,
            outgoing->sourcePortId) != nullptr;
    const float position = hasProbeBefore
            ? 2.f / 3.f
            : (hasProbeAfter ? 1.f / 3.f : 0.5f);
    return source + (destination - source) * position;
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
        const Node& node) {
    if (node.kind != NodeKind::SpectralLayer) {
        return node.bounds;
    }

    const std::optional<Point<float>> centre = inlinePanCentre(graph, node);
    return centre.has_value() ? node.bounds.withCentre(*centre) : node.bounds;
}

int NodeCanvasScene::cableExtraEdgeIndex(const NodeGraph& graph, int edgeIndex) {
    if (!isPositiveAndBelow(edgeIndex, (int) graph.getEdges().size())) {
        return edgeIndex;
    }

    const Edge& edge = graph.getEdges()[(size_t) edgeIndex];
    const Node* sourceNode = findNode(graph, edge.sourceNodeId);
    if (sourceNode == nullptr || sourceNode->kind != NodeKind::SpectralLayer) {
        return edgeIndex;
    }

    const Edge* incoming = singleSignalEdgeForNode(graph, sourceNode->id, true);
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
        uint64_t documentRevision) {
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

    int zOrder = 100;
    for (const auto& node : graph.getNodes()) {
        const Rectangle<float> presentationBounds = presentationWorldBounds(graph, node);
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
                if (ModulationCableBundle::hidesIndividualPort(node, port)
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
                current.targets.push_back({
                        kind,
                        (port.input ? "input:" : "output:") + node.id + "." + port.id,
                        node.id,
                        port.id,
                        {},
                        juce::Rectangle<float>(size, size).withCentre(centre).expanded(hitPadding),
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
                || ModulationCableBundle::supportsDestination(node)) {
            const bool input = node.kind != NodeKind::ModulationTriple;
            const auto centre = viewport.toScreen(
                    ModulationCableBundle::worldCentre(node, input));
            const float size = ModulationCableBundle::socketDiameter
                    * viewport.getZoom() / NodePortGeometry::referenceZoom;
            current.targets.push_back({
                    input ? NodeSceneTargetKind::InputPort : NodeSceneTargetKind::OutputPort,
                    (input ? "input:" : "output:") + node.id + ".modulationBundle",
                    node.id,
                    ModulationCableBundle::portId(),
                    {},
                    juce::Rectangle<float>(size, size).withCentre(centre).expanded(
                            NodePortGeometry::hitPadding),
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
        const Port* sourcePort = findPort(*sourceNode, edge.sourcePortId, false);
        const Port* destinationPort = findPort(*destinationNode, edge.destPortId, true);
        if (sourcePort == nullptr) {
            continue;
        }

        const bool isModulationBundle = modulationBundle.has_value();
        const bool usesSharedModulationSource = isModulationBundle
                || ModulationCableBundle::usesSharedSourceSocket(*sourceNode, edge);
        Node presentedSourceNode = *sourceNode;
        presentedSourceNode.bounds = presentationWorldBounds(graph, *sourceNode);
        Node presentedDestinationNode = *destinationNode;
        presentedDestinationNode.bounds = presentationWorldBounds(graph, *destinationNode);
        const auto source = viewport.toScreen(usesSharedModulationSource
                ? ModulationCableBundle::worldCentre(presentedSourceNode, false)
                : portWorldCentre(presentedSourceNode, *sourcePort));
        const auto attachmentCentre = NodeViewModuleRegistry::instance()
                .moduleFor(destinationNode->kind).attachmentWorldCentre(*destinationNode, edge.destPortId);
        if (destinationPort == nullptr && !attachmentCentre.has_value()) {
            continue;
        }
        const auto destination = viewport.toScreen(isModulationBundle
                ? ModulationCableBundle::worldCentre(presentedDestinationNode, true)
                : destinationPort != nullptr
                ? portWorldCentre(presentedDestinationNode, *destinationPort)
                : *attachmentCentre);
        const PortSide destinationSide = destinationPort != nullptr ? destinationPort->side : PortSide::Top;
        juce::Path visiblePath = cablePath(
                source,
                destination,
                usesSharedModulationSource ? PortSide::Right : sourcePort->side,
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
                destinationNode->kind != NodeKind::SpectralLayer
        });
    }

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

    for (auto edge = scene.edges.rbegin(); edge != scene.edges.rend(); ++edge) {
        if (edge->hitPath.contains(screenPosition)) {
            NodeSceneTarget target;
            target.kind = NodeSceneTargetKind::Edge;
            target.semanticId = "edge:" + juce::String(edge->edgeIndex);
            target.edgeIndex = edge->edgeIndex;
            return target;
        }
    }

    return std::nullopt;
}

}
