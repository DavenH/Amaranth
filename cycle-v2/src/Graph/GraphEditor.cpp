#include "GraphEditor.h"

namespace CycleV2 {

namespace {

PortDomain edgeDomainForConnection(const Port& source, const Port& dest) {
    if (source.domain == PortDomain::ControlSignal && dest.domain != PortDomain::ControlSignal) {
        return dest.domain;
    }

    return source.domain;
}

}

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
            edgeDomainForConnection(*source, *dest),
            dest->purpose == PortPurpose::ScratchAttachment
    });

    auto issues = GraphValidator().validate(candidate);

    if (!issues.empty()) {
        return { GraphEditCode::ValidationRejected, {}, std::move(issues) };
    }

    graph = std::move(candidate);
    return {};
}

GraphEditResult GraphEditor::attachGuideCurveToTrimeshVertexParameter(
        NodeGraph& graph,
        const String& guideNodeId,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const Node* guideNode = findNode(graph, guideNodeId);
    const Node* meshNode = findNode(graph, meshNodeId);

    if (guideNode == nullptr || meshNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    const Port* guideOutput = findPort(*guideNode, "guide", false);

    if (guideOutput == nullptr) {
        return { GraphEditCode::MissingPort, {}, {} };
    }

    const String targetPortId = guideVertexTargetPortId(vertexIndex, parameterField);
    NodeGraph candidate = graph;
    candidate.removeEdgesToInput(meshNodeId, targetPortId);
    candidate.addEdge({
            guideNodeId,
            "guide",
            meshNodeId,
            targetPortId,
            PortDomain::EnvelopeSignal,
            true
    });

    auto issues = GraphValidator().validate(candidate);

    if (!issues.empty()) {
        return { GraphEditCode::ValidationRejected, {}, std::move(issues) };
    }

    graph = std::move(candidate);
    return { GraphEditCode::Connected, guideNodeId, {} };
}

GraphEditResult GraphEditor::createAndAttachGuideCurveToTrimeshVertexParameter(
        NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField,
        Point<float> guidePosition) const {
    NodeGraph candidate = graph;
    GraphEditResult addResult = addNode(candidate, NodeKind::GuideCurve, guidePosition);

    if (!addResult.succeeded()) {
        return addResult;
    }

    GraphEditResult attachResult = attachGuideCurveToTrimeshVertexParameter(
            candidate,
            addResult.nodeId,
            meshNodeId,
            vertexIndex,
            parameterField);

    if (!attachResult.succeeded()) {
        return attachResult;
    }

    graph = std::move(candidate);
    return { GraphEditCode::Connected, addResult.nodeId, {} };
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

GraphEditResult GraphEditor::setNodeParameter(
        NodeGraph& graph,
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) const {
    Node* node = findMutableNode(graph, nodeId);

    if (node == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    for (auto& parameter : node->parameters) {
        if (parameter.id == parameterId) {
            parameter.label = label;
            parameter.value = value;
            return { GraphEditCode::Connected, nodeId, {} };
        }
    }

    node->parameters.push_back({ parameterId, label, value });
    return { GraphEditCode::Connected, nodeId, {} };
}

const Node* GraphEditor::findNode(const NodeGraph& graph, const String& nodeId) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

Node* GraphEditor::findMutableNode(NodeGraph& graph, const String& nodeId) const {
    for (auto& node : graph.getNodesForEditing()) {
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

String GraphEditor::guideVertexTargetPortId(int vertexIndex, const String& parameterField) const {
    return "guide.vertex." + String(vertexIndex) + "." + parameterField;
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
        case NodeKind::WaveSource:                   return "wave";
        case NodeKind::ImageSource:                  return "image";
        case NodeKind::TrilinearMesh:                return "mesh";
        case NodeKind::Fft:                          return "fft";
        case NodeKind::Ifft:                         return "ifft";
        case NodeKind::Envelope:                     return "env";
        case NodeKind::Add:                          return "add";
        case NodeKind::Multiply:                     return "multiply";
        case NodeKind::GuideCurve:                   return "guide";
        case NodeKind::ImpulseResponse:              return "ir";
        case NodeKind::Waveshaper:                   return "waveshaper";
        case NodeKind::Reverb:                       return "reverb";
        case NodeKind::Delay:                        return "delay";
        case NodeKind::StereoSplit:                  return "split";
        case NodeKind::StereoJoin:                   return "join";
        case NodeKind::Output:                       return "out";
        default:                                     return "processor";
    }
}

}
