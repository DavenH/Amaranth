#include "NodeCanvasScene.h"
#include "NodeViewModule.h"

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

juce::Path cablePath(
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
    if (port.side == PortSide::Top || port.side == PortSide::Bottom) {
        const int index = portIndexOnSide(node, port);
        const int count = juce::jmax(1, portCountOnSide(node, port.side));
        return {
                node.bounds.getX() + node.bounds.getWidth()
                        * ((float) index + 1.f) / ((float) count + 1.f),
                port.side == PortSide::Top ? node.bounds.getY() : node.bounds.getBottom()
        };
    }

    const float y = node.bounds.getY() + 58.f + (float) portIndexOnSide(node, port) * 34.f;
    return {
            port.side == PortSide::Right ? node.bounds.getRight() : node.bounds.getX(),
            y
    };
}

const NodeCanvasSceneSnapshot& NodeCanvasScene::build(
        const NodeGraph& graph,
        const NodeCanvasViewport& viewport,
        uint64_t presentationRevision,
        uint64_t documentRevision) {
    const uint64_t graphRevision = documentRevision != 0 ? documentRevision : graph.getRevision();
    if (current.graphRevision == graphRevision
            && current.viewportRevision == viewport.getRevision()
            && current.presentationRevision == presentationRevision) {
        return current;
    }

    current = {};
    current.graphRevision = graphRevision;
    current.viewportRevision = viewport.getRevision();
    current.presentationRevision = presentationRevision;

    int zOrder = 100;
    for (const auto& node : graph.getNodes()) {
        current.targets.push_back({
                NodeSceneTargetKind::Node,
                "node:" + node.id,
                node.id,
                {},
                {},
                viewport.toScreen(node.bounds),
                -1,
                zOrder++
        });

        auto appendPorts = [&](const std::vector<Port>& ports, NodeSceneTargetKind kind) {
            for (const auto& port : ports) {
                const auto centre = viewport.toScreen(portWorldCentre(node, port));
                const float size = 8.8f * viewport.getZoom() / 0.58f;
                current.targets.push_back({
                        kind,
                        (port.input ? "input:" : "output:") + node.id + "." + port.id,
                        node.id,
                        port.id,
                        {},
                        juce::Rectangle<float>(size, size).withCentre(centre).expanded(10.f),
                        -1,
                        10000 + zOrder++
                });
            }
        };
        appendPorts(node.inputs, NodeSceneTargetKind::InputPort);
        appendPorts(node.outputs, NodeSceneTargetKind::OutputPort);
    }

    for (int edgeIndex = 0; edgeIndex < (int) graph.getEdges().size(); ++edgeIndex) {
        const auto& edge = graph.getEdges()[(size_t) edgeIndex];
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

        const auto source = viewport.toScreen(portWorldCentre(*sourceNode, *sourcePort));
        const auto attachmentCentre = NodeViewModuleRegistry::instance()
                .moduleFor(destinationNode->kind).attachmentWorldCentre(*destinationNode, edge.destPortId);
        if (destinationPort == nullptr && !attachmentCentre.has_value()) {
            continue;
        }
        const auto destination = viewport.toScreen(destinationPort != nullptr
                ? portWorldCentre(*destinationNode, *destinationPort)
                : *attachmentCentre);
        const PortSide destinationSide = destinationPort != nullptr ? destinationPort->side : PortSide::Top;
        juce::Path hitPath;
        juce::PathStrokeType(16.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
                .createStrokedPath(hitPath, cablePath(
                        source,
                        destination,
                        sourcePort->side,
                        destinationSide,
                        viewport.getZoom()));
        current.edges.push_back({ edgeIndex, source, destination, std::move(hitPath) });
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
