#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

#include <Curve/Mesh/Mesh.h>

#include "Graph/GraphNodeStateEditor.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

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

void reconcileTrimeshGuideAssignments(
        NodeGraph& graph,
        const String& nodeId,
        GraphEditResult& result) {
    const Node* node = graph.findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return;
    }

    const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model);
    const int cubeCount = model != nullptr ? model->mesh().getNumCubes() : 0;
    if (graph.removeGuideAssignmentsOutsideCubeRange(
            nodeId, cubeCount, &result.changes.removedGuideAssignments) == 0) {
        return;
    }

    result.changes.guidesChanged = true;
    result.changes.guidePresentationChanged = true;
}

int trimeshCubeCount(const NodeModelStatePtr& model) {
    const auto trimesh = std::dynamic_pointer_cast<const TrimeshNodeModelState>(model);
    return trimesh != nullptr ? trimesh->mesh().getNumCubes() : 0;
}

bool validAudioResource(const AudioSampleResource& resource) {
    if (resource.id.isEmpty() || resource.name.isEmpty()
            || !std::isfinite(resource.sampleRate) || resource.sampleRate <= 0.0
            || resource.samples.empty() || resource.samples.size() > 16384) {
        return false;
    }
    return std::all_of(resource.samples.begin(), resource.samples.end(), [](float sample) {
        return std::isfinite(sample) && sample >= -1.f && sample <= 1.f;
    });
}

GraphEditResult replaceOptionalAudioResourceModel(
        const GraphNodeStateEditor& editor,
        NodeGraph& graph,
        NodeAudioResourceEdit& edit) {
    if (edit.model == nullptr) {
        GraphEditResult unchanged;
        unchanged.changed = false;
        return unchanged;
    }
    return editor.replaceNodeModel(
            graph,
            edit.nodeId,
            edit.expectedModelRevision,
            std::move(edit.model));
}

bool bindReplacementAudioResource(
        NodeGraph& graph,
        NodeAudioResourceEdit& edit,
        const String& previousResourceId) {
    const String resourceId = edit.resource.id;
    if (!graph.addAudioResource(std::move(edit.resource))
            || !graph.bindAudioResource({ edit.nodeId, resourceId, std::move(edit.mode) })) {
        return false;
    }
    if (previousResourceId.isNotEmpty()
            && graph.audioResourceUsageCount(previousResourceId) == 0) {
        graph.removeAudioResource(previousResourceId);
    }
    return true;
}

GraphEditResult audioResourceEditResult(
        const String& nodeId,
        ParameterImpact parameterImpacts,
        bool modelChanged) {
    GraphEditResult result;
    result.nodeId = nodeId;
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = parameterImpacts
            | ParameterImpact::Presentation
            | ParameterImpact::Preview
            | ParameterImpact::DspConfiguration;
    result.changes.modelChanged = modelChanged;
    result.changes.resourcesChanged = true;
    return result;
}

}

GraphEditResult GraphNodeStateEditor::setNodeParameter(
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

    if (NodeParameter* parameter = graph.findNodeParameterForEditing(nodeId, parameterId)) {
        const bool effectiveValueEqual = parameterDefinition != nullptr
                        && parameterDefinition->type == ParameterType::Float
                ? parameter->value.getDoubleValue() == normalizedValue.getDoubleValue()
                : parameter->value == normalizedValue;
        if (effectiveValueEqual && parameter->label == resolvedLabel) {
            GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
            result.changed = false;
            return result;
        }
        parameter->label = resolvedLabel;
        parameter->value = normalizedValue;
        graph.markChanged();
        GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
        result.changes.nodeIds.push_back(nodeId);
        result.changes.parameterImpacts = impacts;
        if (node->kind == NodeKind::Envelope && parameterId == "purpose") {
            applyEnvelopePurposeSemantics(graph, *node, result);
        }
        return result;
    }

    graph.addNodeParameter(nodeId, { parameterId, resolvedLabel, normalizedValue });
    GraphEditResult result { GraphEditCode::Connected, nodeId, {} };
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = impacts;
    if (node->kind == NodeKind::Envelope && parameterId == "purpose") {
        applyEnvelopePurposeSemantics(graph, *node, result);
    }
    return result;
}

GraphEditResult GraphNodeStateEditor::setNodeParametersAtomic(
        NodeGraph& graph,
        const String& nodeId,
        const std::vector<NodeParameter>& parameters) const {
    InteractionComplexityDiagnostics::recordParameterLinearScan();
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

GraphEditResult GraphNodeStateEditor::replaceNodeModel(
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

    const int previousCubeCount = trimeshCubeCount(node->model);
    const int nextCubeCount = trimeshCubeCount(model);
    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = graph.replaceNodeModel(nodeId, std::move(model));
    if (result.changed && nextCubeCount < previousCubeCount) {
        reconcileTrimeshGuideAssignments(graph, nodeId, result);
    }
    result.changes.nodeIds.push_back(nodeId);
    result.changes.modelChanged = result.changed;
    result.changes.parameterImpacts = ParameterImpact::Presentation
            | ParameterImpact::Preview
            | ParameterImpact::DspConfiguration;
    return result;
}

GraphEditResult GraphNodeStateEditor::replaceTransientNodeModel(
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

    const int previousCubeCount = trimeshCubeCount(node->model);
    const int nextCubeCount = trimeshCubeCount(model);
    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = graph.replaceNodeModel(nodeId, std::move(model));
    if (result.changed && nextCubeCount < previousCubeCount) {
        reconcileTrimeshGuideAssignments(graph, nodeId, result);
    }
    result.changes.nodeIds.push_back(nodeId);
    result.changes.parameterImpacts = ParameterImpact::Presentation
            | ParameterImpact::Preview
            | ParameterImpact::DspConfiguration;
    return result;
}

GraphEditResult GraphNodeStateEditor::setNodeEditorState(
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

GraphEditResult GraphNodeStateEditor::setNodeAudioResource(
        NodeGraph& graph,
        NodeAudioResourceEdit edit) const {
    if (graph.findNode(edit.nodeId) == nullptr) {
        return { GraphEditCode::MissingNode, edit.nodeId, {} };
    }
    if (!validAudioResource(edit.resource) || edit.mode.isEmpty()
            || graph.findAudioResource(edit.resource.id) != nullptr) {
        return { GraphEditCode::InvalidControlValue, edit.nodeId, {} };
    }

    NodeGraph candidate = graph;
    const NodeAudioResourceBinding* previousBinding = candidate.findAudioResourceBinding(edit.nodeId);
    const String previousResourceId = previousBinding != nullptr
            ? previousBinding->resourceId
            : String();
    const auto parameterResult = setNodeParametersAtomic(
            candidate,
            edit.nodeId,
            edit.parameters);
    if (!parameterResult.succeeded()) {
        return parameterResult;
    }
    const GraphEditResult modelResult = replaceOptionalAudioResourceModel(*this, candidate, edit);
    if (!modelResult.succeeded()) {
        return modelResult;
    }
    if (!bindReplacementAudioResource(candidate, edit, previousResourceId)) {
        return { GraphEditCode::InvalidControlValue, edit.nodeId, {} };
    }

    graph = std::move(candidate);
    return audioResourceEditResult(
            edit.nodeId,
            parameterResult.changes.parameterImpacts,
            modelResult.changed);
}

GraphEditResult GraphNodeStateEditor::removeNodeAudioResource(
        NodeGraph& graph,
        const String& nodeId) const {
    if (graph.findNode(nodeId) == nullptr) {
        return { GraphEditCode::MissingNode, nodeId, {} };
    }
    const NodeAudioResourceBinding* binding = graph.findAudioResourceBinding(nodeId);
    if (binding == nullptr) {
        GraphEditResult unchanged { GraphEditCode::Connected, nodeId, {} };
        unchanged.changed = false;
        return unchanged;
    }

    NodeGraph candidate = graph;
    const String resourceId = binding->resourceId;
    candidate.unbindAudioResource(nodeId);
    if (candidate.audioResourceUsageCount(resourceId) == 0) {
        candidate.removeAudioResource(resourceId);
    }
    graph = std::move(candidate);

    return audioResourceEditResult(nodeId, ParameterImpact::None, false);
}

Node* GraphNodeStateEditor::findMutableNode(NodeGraph& graph, const String& nodeId) const {
    return graph.findNodeForEditing(nodeId);
}

}
