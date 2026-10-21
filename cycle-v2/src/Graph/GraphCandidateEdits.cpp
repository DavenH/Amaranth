#include "Graph/GraphCommandDispatcher.h"

#include <set>

#include "Graph/GraphValidator.h"
#include "Graph/GraphNodeStateEditor.h"

namespace CycleV2 {

namespace {

GraphEditResult rejected(GraphEditCode code, const String& message,
        int editIndex = -1, const String& nodeId = {}) {
    GraphEditResult result { code, nodeId, {} };
    result.changed = false;
    result.message = message;
    result.editIndex = editIndex;
    return result;
}

}

GraphEditResult GraphCommandDispatcher::applyNodeEdits(const NodeAuthoringEdits& request) {
    if (compoundActive || transientEdit.has_value()) {
        return rejected(GraphEditCode::ValidationRejected, "An edit gesture is already active");
    }
    if (request.expectedDocumentRevision != document.revision()) {
        return rejected(GraphEditCode::StaleRevision, "Document revision has changed");
    }
    if (request.edits.empty() || request.edits.size() > maximumNodeAuthoringEdits) {
        return rejected(GraphEditCode::ValidationRejected, "Requires 1 to 64 node edits");
    }
    const auto& registry = NodeDefinitionRegistry::instance();
    std::set<String> nodeIds;
    std::vector<NodeModelStatePtr> models(request.edits.size());
    size_t parameterCount = 0;
    for (size_t index = 0; index < request.edits.size(); ++index) {
        const auto& edit = request.edits[index];
        const auto fail = [&](GraphEditCode code, const String& message) {
            return rejected(code, message, (int) index, edit.nodeId);
        };
        if (!nodeIds.insert(edit.nodeId).second) {
            return fail(GraphEditCode::ValidationRejected, "Node occurs more than once");
        }
        const Node* node = document.graph().findNode(edit.nodeId);
        if (node == nullptr) {
            return fail(GraphEditCode::MissingNode, "Node does not exist");
        }
        const bool replacingModel = edit.expectedModelRevision.has_value() || !edit.model.isVoid();
        if (edit.parameters.empty() && !replacingModel) {
            return fail(GraphEditCode::ValidationRejected, "Node edit has no stored changes");
        }
        parameterCount += edit.parameters.size();
        if (parameterCount > maximumNodeAuthoringParameters) {
            return fail(GraphEditCode::ValidationRejected, "Too many parameter edits");
        }
        std::set<String> parameterIds;
        for (const auto& parameter : edit.parameters) {
            const auto* definition = registry.findParameter(node->kind, parameter.id);
            if (definition == nullptr) {
                return fail(GraphEditCode::UnknownParameter, "Unknown parameter: " + parameter.id);
            }
            if (!parameterIds.insert(parameter.id).second
                    || !definition->acceptsAuthoringValue(parameter.value)) {
                return fail(GraphEditCode::InvalidParameterValue,
                        "Invalid or repeated parameter: " + parameter.id);
            }
        }
        if (replacingModel) {
            if (!edit.expectedModelRevision.has_value() || edit.model.isVoid()) {
                return fail(GraphEditCode::InvalidTypedSnapshot,
                        "Model and expectedModelRevision are both required");
            }
            const auto* definition = registry.find(node->kind);
            const auto codec = definition != nullptr ? definition->modelCodec : nullptr;
            if (codec == nullptr || !codec->supportsAuthoring()) {
                return fail(GraphEditCode::WrongNodeKind, "Node does not support authored models");
            }
            const uint64_t current = node->model != nullptr ? node->model->revision() : 0;
            if (*edit.expectedModelRevision != current) {
                return fail(GraphEditCode::StaleRevision, "Model revision has changed");
            }
            String error;
            models[index] = codec->readAuthoringJSON(edit.model, error);
            if (models[index] == nullptr) {
                return fail(GraphEditCode::InvalidTypedSnapshot, error);
            }
        }
    }

    beginCompoundEdit();
    for (size_t index = 0; index < request.edits.size(); ++index) {
        const auto& edit = request.edits[index];
        GraphEditResult result;
        if (!edit.parameters.empty()) {
            result = applyIncremental(
                    [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                        for (const auto& parameter : edit.parameters) {
                            delta.captureNodeParameter(graph, edit.nodeId, parameter.id);
                        }
                    },
                    [&](NodeGraph& graph) {
                        return GraphNodeStateEditor().setNodeParametersAtomic(
                                graph, edit.nodeId, edit.parameters);
                    });
        }
        if (result.succeeded() && models[index] != nullptr) {
            result = replaceNodeModel(edit.nodeId, *edit.expectedModelRevision, models[index]);
        }
        if (!result.succeeded()) {
            cancelCompoundEdit();
            result.changed = false;
            result.editIndex = (int) index;
            result.message = "Stored node edit was rejected";
            return result;
        }
    }
    const auto issues = GraphValidator().validate(document.graph());
    if (!issues.empty()) {
        cancelCompoundEdit();
        auto result = rejected(GraphEditCode::ValidationRejected,
                "Candidate does not form a valid graph");
        result.validationIssues = issues;
        return result;
    }
    GraphEditResult result;
    result.changes = compoundChanges;
    result.changed = commitCompoundEdit();
    return result;
}

}
