#include "GraphEditor.h"

#include "../Nodes/Effect2D/CurveNodeModels.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../Nodes/Envelope/EnvelopePurpose.h"

#include <unordered_map>

namespace CycleV2 {

namespace {

PortDomain edgeDomainForConnection(const Port& source, const Port& dest) {
    if (source.domain == PortDomain::ControlSignal && dest.domain != PortDomain::ControlSignal) {
        return dest.domain;
    }

    return source.domain;
}

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

bool isProbeDomain(PortDomain domain) {
    return domain == PortDomain::TimeSignal
            || domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal;
}

void applyEnvelopePurposeSemantics(
        NodeGraph& graph,
        Node& node,
        GraphEditResult& result) {
    NodeDefinitionRegistry::instance().normalize(node);
    const Port& output = node.outputs.front();
    for (size_t index = graph.getEdges().size(); index > 0; --index) {
        const Edge& edge = graph.getEdges()[index - 1];
        if (edge.sourceNodeId != node.id || edge.sourcePortId != output.id) {
            continue;
        }
        const bool compatible = edge.connectionKind == output.connectionKind
                && (output.connectionKind == ConnectionKind::Signal
                        ? (output.domain == PortDomain::ControlSignal
                                || edge.domain == output.domain)
                        : edge.attachmentType == output.attachmentType);
        if (compatible) {
            continue;
        }
        result.changes.removedEdges.push_back(edge);
        graph.removeEdgeAt(index - 1);
    }
    result.changes.topologyChanged = !result.changes.removedEdges.empty();
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
                    ? ConnectionKind::ProcessingAttachment
                    : dest->connectionKind,
            dest->purpose == PortPurpose::ScratchAttachment
                    ? AttachmentType::ScratchEnvelope
                    : dest->attachmentType
    });

    auto issues = GraphValidator().validate(candidate);

    if (!issues.empty()) {
        return { GraphEditCode::ValidationRejected, {}, std::move(issues) };
    }

    graph = std::move(candidate);
    return {};
}

GraphEditResult GraphEditor::createGuideCurve(NodeGraph& graph) const {
    int nextNumber = 1;
    while (graph.findGuideCurve("guide" + String(nextNumber)) != nullptr) {
        ++nextNumber;
    }

    GuideCurveResource guide;
    guide.id = "guide" + String(nextNumber);
    guide.shortLabel = "G" + String(nextNumber);
    guide.colourIndex = nextNumber - 1;
    guide.shelfOrder = (int) graph.getGuideCurves().size();
    guide.model = createDefaultGuideCurveModel();
    if (!graph.addGuideCurve(std::move(guide))) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }
    return { GraphEditCode::Connected, "guide" + String(nextNumber), {} };
}

GraphEditResult GraphEditor::removeGuideCurve(NodeGraph& graph, const String& guideId) const {
    if (!graph.removeGuideCurve(guideId)) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    return { GraphEditCode::Connected, guideId, {} };
}

GraphEditResult GraphEditor::replaceGuideCurve(
        NodeGraph& graph,
        const String& guideId,
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls) const {
    GuideCurveResource* guide = graph.findGuideCurveForEditing(guideId);
    if (guide == nullptr || model == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }

    for (const auto& control : controls) {
        if (control.id == "enabled") {
            guide->enabled = control.value.getIntValue() != 0;
        } else if (control.id == "noise") {
            guide->noise = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        } else if (control.id == "dcOffset") {
            guide->dcOffset = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        } else if (control.id == "phase") {
            guide->phase = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        }
    }
    guide->model = std::move(model);
    graph.markChanged();
    return { GraphEditCode::Connected, guideId, {} };
}

GraphEditResult GraphEditor::assignGuideCurveToTrimeshVertexParameter(
        NodeGraph& graph,
        const String& guideId,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const GuideCurveResource* guide = graph.findGuideCurve(guideId);
    const Node* meshNode = findNode(graph, meshNodeId);
    if (guide == nullptr || meshNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }
    if (meshNode->kind != NodeKind::TrilinearMesh
            || std::find(
                    TrimeshGuideAttachmentTarget::fields().begin(),
                    TrimeshGuideAttachmentTarget::fields().end(),
                    parameterField) == TrimeshGuideAttachmentTarget::fields().end()) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const auto targetPortIds = TrimeshGuideAttachmentTarget::cubePortIdsForVertex(
            *meshNode, vertexIndex, parameterField);
    if (targetPortIds.empty()) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }
    for (const auto& targetPortId : targetPortIds) {
        const auto target = TrimeshGuideAttachmentTarget::parse(targetPortId);
        if (!target.isValid()) {
            return { GraphEditCode::ValidationRejected, {}, {} };
        }

        const TrimeshCubeComponentGuideTarget componentTarget {
                target.cubeIndex,
                TrimeshGuideAttachmentTarget::guideField(target.field)
        };
        const auto existing = std::find_if(
                graph.getGuideAssignments().begin(),
                graph.getGuideAssignments().end(),
                [&](const GuideCurveAssignment& assignment) {
                    return assignment.guideId == guideId
                            && assignment.targets(meshNodeId, componentTarget);
                });
        if (existing == graph.getGuideAssignments().end()
                && !graph.assignGuideCurve({ guideId, meshNodeId, componentTarget })) {
            return { GraphEditCode::ValidationRejected, {}, {} };
        }
    }
    return { GraphEditCode::Connected, guideId, {} };
}

GraphEditResult GraphEditor::createGuideCurveAndAssignToTrimeshVertexParameter(
        NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const auto created = createGuideCurve(graph);
    if (!created.succeeded()) {
        return created;
    }
    return assignGuideCurveToTrimeshVertexParameter(
            graph, created.nodeId, meshNodeId, vertexIndex, parameterField);
}

GraphEditResult GraphEditor::toggleSignalProbe(
        NodeGraph& graph,
        size_t edgeIndex,
        float tapPosition) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, {}, {} };
    }

    const Edge& edge = graph.getEdges()[edgeIndex];
    if (edge.isAttachment()
            || !isProbeDomain(GraphValidator().resolvedDomainForEdge(graph, edge))) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    if (const auto* existing = graph.findSignalProbeForSource(
                edge.sourceNodeId, edge.sourcePortId)) {
        const String probeId = existing->id;
        graph.removeSignalProbe(probeId);
        return { GraphEditCode::Connected, probeId, {} };
    }

    const String probeId = createUniqueProbeId(graph);
    int nextRailOrder {};
    for (const auto& probe : graph.getSignalProbes()) {
        nextRailOrder = jmax(nextRailOrder, probe.railOrder + 1);
    }
    graph.addSignalProbe({
            probeId,
            edge.sourceNodeId,
            edge.sourcePortId,
            edge.destNodeId,
            edge.destPortId,
            "Spy " + String(nextRailOrder + 1),
            jlimit(0.f, 1.f, tapPosition),
            nextRailOrder
    });
    return { GraphEditCode::Connected, probeId, {} };
}

GraphEditResult GraphEditor::removeSignalProbe(NodeGraph& graph, const String& probeId) const {
    if (!graph.removeSignalProbe(probeId)) {
        return { GraphEditCode::MissingNode, probeId, {} };
    }
    return { GraphEditCode::Connected, probeId, {} };
}

GraphEditResult GraphEditor::reattachSignalProbe(
        NodeGraph& graph,
        const String& probeId,
        size_t edgeIndex,
        float tapPosition) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge, probeId, {} };
    }
    SignalProbe* probe = graph.findSignalProbeForEditing(probeId);
    if (probe == nullptr) {
        return { GraphEditCode::MissingNode, probeId, {} };
    }

    const Edge& edge = graph.getEdges()[edgeIndex];
    if (edge.isAttachment()
            || !isProbeDomain(GraphValidator().resolvedDomainForEdge(graph, edge))) {
        return { GraphEditCode::ValidationRejected, probeId, {} };
    }
    const SignalProbe* existing = graph.findSignalProbeForSource(
            edge.sourceNodeId, edge.sourcePortId);
    if (existing != nullptr && existing->id != probeId) {
        return { GraphEditCode::ValidationRejected, probeId, {} };
    }

    probe->sourceNodeId = edge.sourceNodeId;
    probe->sourcePortId = edge.sourcePortId;
    probe->anchorDestNodeId = edge.destNodeId;
    probe->anchorDestPortId = edge.destPortId;
    probe->tapPosition = jlimit(0.f, 1.f, tapPosition);
    graph.markChanged();
    return { GraphEditCode::Connected, probeId, {} };
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
            const bool effectiveValueEqual = parameterDefinition != nullptr
                            && parameterDefinition->type == ParameterType::Float
                    ? parameter.value.getDoubleValue() == normalizedValue.getDoubleValue()
                    : parameter.value == normalizedValue;
            if (effectiveValueEqual && parameter.label == resolvedLabel) {
                GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
                result.changed = false;
                return result;
            }
            parameter.label = resolvedLabel;
            parameter.value = normalizedValue;
            graph.markChanged();
            GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
            result.changes.nodeIds.push_back(nodeId);
            result.changes.parameterImpacts = impacts;
            if (node->kind == NodeKind::Envelope && parameterId == "purpose") {
                applyEnvelopePurposeSemantics(graph, *node, result);
            }
            return result;
        }
    }

    node->parameters.push_back({ parameterId, resolvedLabel, normalizedValue });
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = impacts;
    if (node->kind == NodeKind::Envelope && parameterId == "purpose") {
        applyEnvelopePurposeSemantics(graph, *node, result);
    }
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
    std::unordered_map<String, size_t, StringHash> normalizedIndices;
    normalizedIndices.reserve(parameters.size());
    ParameterImpact impacts = ParameterImpact::None;
    for (const auto& parameter : parameters) {
        const auto* definition = registry.findParameter(node->kind, parameter.id);
        if (definition == nullptr || !definition->accepts(parameter.value)) {
            return { GraphEditCode::InvalidControlValue, nodeId, {} };
        }
        if (!normalizedIndices.emplace(parameter.id, normalized.size()).second) {
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
    std::unordered_map<String, size_t, StringHash> nextIndices;
    nextIndices.reserve(nextParameters.size() + normalized.size());
    for (size_t index = 0; index < nextParameters.size(); ++index) {
        nextIndices.emplace(nextParameters[index].id, index);
    }

    for (const auto& parameter : normalized) {
        const auto existing = nextIndices.find(parameter.id);
        if (existing != nextIndices.end()) {
            nextParameters[existing->second] = parameter;
        } else {
            nextIndices.emplace(parameter.id, nextParameters.size());
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
    if (node->kind == NodeKind::Envelope
            && normalizedIndices.find("purpose") != normalizedIndices.end()) {
        applyEnvelopePurposeSemantics(graph, *node, result);
    }
    return result;
}

GraphEditResult GraphEditor::replaceNodeModel(
        NodeGraph& graph,
        const String& nodeId,
        uint64_t expectedRevision,
        NodeModelStatePtr model) const {
    Node* node = findMutableNode(graph, nodeId);
    if (node == nullptr) {
        return { GraphEditCode::MissingNode, nodeId, {} };
    }
    if (model == nullptr) {
        return { GraphEditCode::InvalidTypedSnapshot, nodeId, {} };
    }

    const uint64_t currentRevision = node->model != nullptr ? node->model->revision() : 0;
    if (currentRevision != expectedRevision) {
        return { GraphEditCode::StaleRevision, nodeId, {} };
    }
    if (node->model != nullptr && node->model->schemaId() != model->schemaId()) {
        return { GraphEditCode::WrongNodeKind, nodeId, {} };
    }
    if (model->revision() < currentRevision) {
        return { GraphEditCode::StaleRevision, nodeId, {} };
    }
    if (model->revision() == currentRevision && !node->model->equals(*model)) {
        return { GraphEditCode::ConflictingRevision, nodeId, {} };
    }

    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = graph.replaceNodeModel(nodeId, std::move(model));
    result.changes.nodeIds.push_back(nodeId);
    result.changes.modelChanged = result.changed;
    result.changes.parameterImpacts = ParameterImpact::Presentation
            | ParameterImpact::Preview
            | ParameterImpact::DspConfiguration;
    return result;
}

GraphEditResult GraphEditor::replaceTransientNodeModel(
        NodeGraph& graph,
        const String& nodeId,
        uint64_t expectedRevision,
        NodeModelStatePtr model) const {
    Node* node = findMutableNode(graph, nodeId);
    if (node == nullptr) {
        return { GraphEditCode::MissingNode, nodeId, {} };
    }
    if (model == nullptr) {
        return { GraphEditCode::InvalidTypedSnapshot, nodeId, {} };
    }

    const uint64_t currentRevision = node->model != nullptr ? node->model->revision() : 0;
    if (currentRevision != expectedRevision || model->revision() != currentRevision) {
        return { GraphEditCode::StaleRevision, nodeId, {} };
    }
    if (node->model != nullptr && node->model->schemaId() != model->schemaId()) {
        return { GraphEditCode::WrongNodeKind, nodeId, {} };
    }

    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = graph.replaceNodeModel(nodeId, std::move(model));
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = ParameterImpact::Presentation
            | ParameterImpact::Preview
            | ParameterImpact::DspConfiguration;
    return result;
}

GraphEditResult GraphEditor::setNodeEditorState(
        NodeGraph& graph,
        const String& nodeId,
        var editorState) const {
    if (findMutableNode(graph, nodeId) == nullptr) {
        return { GraphEditCode::MissingNode, nodeId, {} };
    }

    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = graph.replaceNodeEditorState(nodeId, std::move(editorState));
    result.changes.nodeIds.push_back(nodeId);
    result.changes.editorStateChanged = result.changed;
    result.changes.parameterImpacts = ParameterImpact::Presentation;
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

String GraphEditor::createUniqueProbeId(const NodeGraph& graph) const {
    String candidate = "probe";
    int suffix = 2;

    while (graph.findSignalProbe(candidate) != nullptr) {
        candidate = "probe" + String(suffix);
        ++suffix;
    }

    return candidate;
}

String GraphEditor::baseIdForKind(NodeKind kind) const {
    const auto* definition = NodeDefinitionRegistry::instance().find(kind);
    return definition != nullptr ? definition->defaultInstanceIdPrefix : "processor";
}

}
