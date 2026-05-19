#include "GraphEditor.h"

namespace CycleV2 {

GraphEditResult GraphEditor::addNode(NodeGraph& graph, NodeKind kind, Point<float> position) const {
    const String nodeId = createUniqueNodeId(graph, kind);
    graph.addNode(GraphNodeFactory().createNode(kind, nodeId, position));
    return { GraphEditCode::Connected, nodeId, {} };
}

GraphEditResult GraphEditor::connect(
        NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) const {
    if (first.input == second.input) {
        return { GraphEditCode::DirectionMismatch, {}, {} };
    }

    const PortAddress& sourceAddress = first.input ? second : first;
    const PortAddress& destAddress = first.input ? first : second;

    const Node* sourceNode = findNode(graph, sourceAddress.nodeId);
    const Node* destNode = findNode(graph, destAddress.nodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    const Port* source = findPort(*sourceNode, sourceAddress.portId, false);
    const Port* dest = findPort(*destNode, destAddress.portId, true);

    if (source == nullptr || dest == nullptr) {
        return { GraphEditCode::MissingPort, {}, {} };
    }

    NodeGraph candidate = graph;
    candidate.removeEdgesToInput(destAddress.nodeId, destAddress.portId);
    candidate.addEdge({
            sourceAddress.nodeId,
            sourceAddress.portId,
            destAddress.nodeId,
            destAddress.portId,
            source->domain,
            dest->purpose == PortPurpose::ScratchAttachment
    });

    auto issues = GraphValidator().validate(candidate);

    if (!issues.empty()) {
        return { GraphEditCode::ValidationRejected, {}, std::move(issues) };
    }

    graph = std::move(candidate);
    return {};
}

GraphEditResult GraphEditor::removeNode(NodeGraph& graph, const String& nodeId) const {
    if (findNode(graph, nodeId) == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    graph.removeNode(nodeId);
    return {};
}

GraphEditResult GraphEditor::removeEdgeAt(NodeGraph& graph, size_t index) const {
    if (index >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    graph.removeEdgeAt(index);
    return {};
}

const Node* GraphEditor::findNode(const NodeGraph& graph, const String& nodeId) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

const Port* GraphEditor::findPort(const Node& node, const String& portId, bool input) const {
    const auto& ports = input ? node.inputs : node.outputs;

    for (const auto& port : ports) {
        if (port.id == portId) {
            return &port;
        }
    }

    return nullptr;
}

String GraphEditor::createUniqueNodeId(const NodeGraph& graph, NodeKind kind) const {
    const String baseId = baseIdForKind(kind);
    String candidate = baseId;
    int suffix = 2;

    while (findNode(graph, candidate) != nullptr) {
        candidate = baseId + String(suffix);
        ++suffix;
    }

    return candidate;
}

String GraphEditor::baseIdForKind(NodeKind kind) const {
    switch (kind) {
        case NodeKind::VoiceContext:                 return "voice";
        case NodeKind::TrilinearWaveSurface:         return "wave";
        case NodeKind::TrilinearMesh:                return "mesh";
        case NodeKind::Fft:                          return "fft";
        case NodeKind::SpectralMagnitudeProcessor:   return "mag";
        case NodeKind::SpectralPhaseProcessor:       return "phase";
        case NodeKind::Ifft:                         return "ifft";
        case NodeKind::Envelope:                     return "env";
        case NodeKind::Add:                          return "add";
        case NodeKind::Multiply:                     return "multiply";
        case NodeKind::StereoSplit:                  return "split";
        case NodeKind::StereoJoin:                   return "join";
        case NodeKind::Output:                       return "out";
        default:                                     return "processor";
    }
}

}
