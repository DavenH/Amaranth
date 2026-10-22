#include "App/Automation/NodeAuthoringAutomation.h"

#include "App/Automation/CycleV2AutomationProtocol.h"

namespace CycleV2 {

namespace {

bool positiveRevision(const var& value) {
    return (value.isInt() || value.isInt64()) && (int64) value > 0;
}

}

bool decodeNodeAuthoringEdits(const var& command, NodeAuthoringEdits& request, String& error) {
    const auto* edits = command["edits"].getArray();
    if (command.getDynamicObject() == nullptr || !positiveRevision(command["expectedDocumentRevision"])
            || edits == nullptr || edits->isEmpty() || (size_t) edits->size() > maximumNodeAuthoringEdits) {
        error = "Requires a positive integer expectedDocumentRevision and 1 to 64 edits";
        return false;
    }
    NodeAuthoringEdits decoded;
    decoded.expectedDocumentRevision = (uint64_t) (int64) command["expectedDocumentRevision"];
    size_t parameterCount = 0;
    for (const auto& value : *edits) {
        NodeAuthoringEdit edit;
        const auto* parameters = value["parameters"].getArray();
        const bool hasModel = value.hasProperty("model");
        if (value.getDynamicObject() == nullptr || !value["nodeId"].isString()
                || value["nodeId"].toString().isEmpty()
                || (value.hasProperty("parameters") && parameters == nullptr)
                || (!hasModel && value.hasProperty("expectedModelRevision"))
                || (hasModel && (!positiveRevision(value["expectedModelRevision"])
                        || value["model"].getDynamicObject() == nullptr))) {
            error = "Each edit requires nodeId, parameter strings and/or a model with expectedModelRevision";
            return false;
        }
        edit.nodeId = value["nodeId"].toString();
        if (parameters != nullptr) {
            parameterCount += (size_t) parameters->size();
            if (parameterCount > maximumNodeAuthoringParameters) {
                error = "At most 1024 parameter edits are allowed";
                return false;
            }
            for (const auto& parameter : *parameters) {
                if (parameter.getDynamicObject() == nullptr || !parameter["id"].isString()
                        || !parameter["value"].isString() || parameter["id"].toString().isEmpty()) {
                    error = "Parameters require nonempty string id and stored string value";
                    return false;
                }
                edit.parameters.push_back({ parameter["id"].toString(), {}, parameter["value"].toString() });
            }
        }
        if (hasModel) {
            edit.expectedModelRevision = (uint64_t) (int64) value["expectedModelRevision"];
            edit.model = value["model"];
        }
        decoded.edits.push_back(std::move(edit));
    }
    request = std::move(decoded);
    return true;
}

var nodeAuthoringResult(const GraphEditResult& result, uint64_t documentRevision) {
    using namespace AutomationProtocol;
    var response = result.succeeded() ? okResult("applyNodeEdits")
            : failedResult("applyNodeEdits", result.message);
    var data = makeObject();
    auto& fields = *objectFor(data);
    fields.setProperty("changed", result.changed);
    fields.setProperty("documentRevision", (int64) documentRevision);
    if (!result.succeeded()) {
        fields.setProperty("code", (int) result.code);
        fields.setProperty("editIndex", result.editIndex);
        fields.setProperty("nodeId", result.nodeId);
        Array<var> issues;
        for (const auto& issue : result.validationIssues) {
            issues.add(issue.message);
        }
        fields.setProperty("validationIssues", issues);
    }
    objectFor(response)->setProperty("data", data);
    return response;
}

}
