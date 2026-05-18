#include "GraphEditor.h"

namespace CycleV2 {

GraphEditResult GraphEditor::connect(
        NodeGraph& graph,
        const PortAddress& first,
        const PortAddress& second) const {
    if (first.input == second.input) {
        return { GraphEditCode::DirectionMismatch, {} };
    }

    const PortAddress& sourceAddress = first.input ? second : first;
    const PortAddress& destAddress = first.input ? first : second;

    const Node* sourceNode = findNode(graph, sourceAddress.nodeId);
    const Node* destNode = findNode(graph, destAddress.nodeId);

    if (sourceNode == nullptr || destNode == nullptr) {
        return { GraphEditCode::MissingNode, {} };
    }

    const Port* source = findPort(*sourceNode, sourceAddress.portId, false);
    const Port* dest = findPort(*destNode, destAddress.portId, true);

    if (source == nullptr || dest == nullptr) {
        return { GraphEditCode::MissingPort, {} };
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
        return { GraphEditCode::ValidationRejected, std::move(issues) };
    }

    graph = std::move(candidate);
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

}
