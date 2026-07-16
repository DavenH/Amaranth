#include "GraphEditor.h"

#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"

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

GraphEditResult GraphEditor::attachSpyToEdge(NodeGraph& graph, size_t edgeIndex, const String& spyNodeId) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    const Edge edge = graph.getEdges()[edgeIndex];
    const Node* spyNode = findNode(graph, spyNodeId);

    if (spyNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    if (spyNode->kind != NodeKind::Spy) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const Port* spyInput = findPort(*spyNode, "in", true);

    if (spyInput == nullptr) {
        return { GraphEditCode::MissingPort, {}, {} };
    }

    NodeGraph candidate = graph;
    GraphEditResult result = connect(
            candidate,
            { edge.sourceNodeId, edge.sourcePortId, false },
            { spyNodeId, spyInput->id, true });

    if (!result.succeeded()) {
        return result;
    }

    graph = std::move(candidate);
    return { GraphEditCode::Connected, spyNodeId, {} };
}

GraphEditResult GraphEditor::spliceNodeIntoEdge(NodeGraph& graph, size_t edgeIndex, const String& nodeId) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    const Edge edge = graph.getEdges()[edgeIndex];

    if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const Node* spliceNode = findNode(graph, nodeId);

    if (spliceNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    if (spliceNode->kind == NodeKind::Spy) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const PortAddress source { edge.sourceNodeId, edge.sourcePortId, false };
    const PortAddress dest { edge.destNodeId, edge.destPortId, true };

    for (const auto& input : spliceNode->inputs) {
        if (!input.input) {
            continue;
        }

        for (const auto& output : spliceNode->outputs) {
            if (output.input) {
                continue;
            }

            NodeGraph candidate = graph;
            candidate.removeEdgeAt(edgeIndex);

            GraphEditResult inResult = connect(candidate, source, { nodeId, input.id, true });

            if (!inResult.succeeded()) {
                continue;
            }

            GraphEditResult outResult = connect(candidate, { nodeId, output.id, false }, dest);

            if (!outResult.succeeded()) {
                continue;
            }

            graph = std::move(candidate);
            return { GraphEditCode::Connected, nodeId, {} };
        }
    }

    return { GraphEditCode::ValidationRejected, {}, {} };
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

    const auto& registry = NodeDefinitionRegistry::instance();
    const auto* nodeDefinition = registry.find(node->kind);
    const auto* parameterDefinition = registry.findParameter(node->kind, parameterId);
    if (parameterDefinition == nullptr
            && (nodeDefinition == nullptr || !nodeDefinition->allowsDynamicParameters)) {
        return { GraphEditCode::UnknownParameter, nodeId, {} };
    }
    if (parameterDefinition != nullptr && !parameterDefinition->accepts(value)) {
        return { GraphEditCode::InvalidParameterValue, nodeId, {} };
    }

    const String normalizedValue = parameterDefinition != nullptr
            ? parameterDefinition->normalized(value)
            : value;
    const String resolvedLabel = parameterDefinition != nullptr ? parameterDefinition->label : label;
    const ParameterImpact impacts = parameterDefinition != nullptr
            ? parameterDefinition->impacts
            : ParameterImpact::Preview | ParameterImpact::DspConfiguration;

    for (auto& parameter : node->parameters) {
        if (parameter.id == parameterId) {
            parameter.label = resolvedLabel;
            parameter.value = normalizedValue;
            graph.markChanged();
            GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
            result.changes.nodeIds.push_back(nodeId);
            result.changes.parameterImpacts = impacts;
            return result;
        }
    }

    node->parameters.push_back({ parameterId, resolvedLabel, normalizedValue });
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = impacts;
    return result;
}

GraphEditResult GraphEditor::setNodeParametersAtomic(
        NodeGraph& graph,
        const String& nodeId,
        const std::vector<NodeParameter>& parameters) const {
    Node* node = findMutableNode(graph, nodeId);
    if (node == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }

    const auto& registry = NodeDefinitionRegistry::instance();
    std::vector<NodeParameter> normalized;
    normalized.reserve(parameters.size());
    ParameterImpact impacts = ParameterImpact::None;
    for (const auto& parameter : parameters) {
        const auto* definition = registry.findParameter(node->kind, parameter.id);
        if (definition == nullptr || !definition->accepts(parameter.value)) {
            return { GraphEditCode::InvalidControlValue, nodeId, {} };
        }
        const auto duplicate = std::find_if(normalized.begin(), normalized.end(), [&](const auto& item) {
            return item.id == parameter.id;
        });
        if (duplicate != normalized.end()) {
            return { GraphEditCode::InvalidControlValue, nodeId, {} };
        }
        normalized.push_back({
                parameter.id,
                definition->label,
                definition->normalized(parameter.value)
        });
        impacts = impacts | definition->impacts;
    }

    auto nextParameters = node->parameters;
    for (const auto& parameter : normalized) {
        const auto existing = std::find_if(nextParameters.begin(), nextParameters.end(), [&](const auto& item) {
            return item.id == parameter.id;
        });
        if (existing != nextParameters.end()) {
            *existing = parameter;
        } else {
            nextParameters.push_back(parameter);
        }
    }
    const bool unchanged = nextParameters.size() == node->parameters.size()
            && std::equal(nextParameters.begin(), nextParameters.end(), node->parameters.begin(),
                    [](const auto& left, const auto& right) {
                        return left.id == right.id && left.label == right.label && left.value == right.value;
                    });
    if (unchanged) {
        GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
        result.changed = false;
        return result;
    }

    node->parameters = std::move(nextParameters);
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = impacts;
    return result;
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
    return graph.findNodeForEditing(nodeId);
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
    return TrimeshGuideAttachmentTarget::portIdFor(vertexIndex, parameterField);
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
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    return definition != nullptr ? definition->defaultInstanceIdPrefix : "processor";
}

}
