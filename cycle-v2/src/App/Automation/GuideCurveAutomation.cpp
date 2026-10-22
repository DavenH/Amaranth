#include <cmath>
#include <limits>

#include "App/Automation/GuideCurveAutomation.h"

#include "App/Automation/CycleV2AutomationProtocol.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {

using namespace AutomationProtocol;

namespace {

bool positiveInteger(const var& value) {
    return (value.isInt() || value.isInt64()) && (int64) value > 0;
}

bool optionalCoordinate(const var& request, const char* field, std::optional<float>& output) {
    if (!request.hasProperty(field)) {
        return true;
    }
    const auto value = request[field];
    if ((!value.isInt() && !value.isInt64() && !value.isDouble())
            || !std::isfinite((double) value)
            || (double) value < 0.0 || (double) value > 1.0) {
        return false;
    }
    output = (float) (double) value;
    return true;
}

bool decodePointEdits(const var& request, GuideCurveEdits& decoded) {
    const auto* edits = request["edits"].getArray();
    if (!positiveInteger(request["expectedModelRevision"]) || edits == nullptr
            || edits->isEmpty() || (size_t) edits->size() > maximumGuideCurvePointEdits) {
        return false;
    }
    decoded.guideId = request["guideId"].toString();
    decoded.expectedDocumentRevision = (uint64_t) (int64) request["expectedDocumentRevision"];
    decoded.expectedModelRevision = (uint64_t) (int64) request["expectedModelRevision"];
    for (const auto& entry : *edits) {
        if (entry.getDynamicObject() == nullptr || !entry["op"].isString()) {
            return false;
        }
        GuideCurvePointEdit edit;
        const String operation = entry["op"].toString();
        if (operation == "set") {
            edit.kind = GuideCurvePointEditKind::Set;
        } else if (operation == "insert") {
            edit.kind = GuideCurvePointEditKind::Insert;
        } else if (operation == "remove") {
            edit.kind = GuideCurvePointEditKind::Remove;
        } else {
            return false;
        }
        if (entry.hasProperty("vertexId")) {
            if (!positiveInteger(entry["vertexId"])) {
                return false;
            }
            edit.vertexId = (uint64_t) (int64) entry["vertexId"];
        }
        if (!optionalCoordinate(entry, "x", edit.x)
                || !optionalCoordinate(entry, "y", edit.y)
                || !optionalCoordinate(entry, "curve", edit.curve)) {
            return false;
        }
        decoded.edits.push_back(std::move(edit));
    }
    return true;
}

var inspectGuide(const GraphDocument& document, const String& guideId) {
    const auto& graph = document.graph();
    const auto* guide = graph.findGuideCurve(guideId);
    if (guide == nullptr || guide->model == nullptr) {
        return {};
    }
    var data = makeObject();
    auto& fields = *objectFor(data);
    fields.setProperty("guideId", guide->id);
    fields.setProperty("documentRevision", (int64) document.revision());
    fields.setProperty("resourceRevision", (int64) guide->revision);
    fields.setProperty("modelRevision", (int64) guide->model->revision());
    fields.setProperty("name", guide->name);
    fields.setProperty("enabled", guide->enabled);
    fields.setProperty("noise", guide->noise);
    fields.setProperty("dcOffset", guide->dcOffset);
    fields.setProperty("phase", guide->phase);
    fields.setProperty("heatmapAssetId", guide->heatmapAssetId);
    const var model = guide->model->writeJSON();
    fields.setProperty("model", model);
    fields.setProperty("points", model["state"]["vertices"]);
    Array<var> assignments;
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.guideId != guideId) {
            continue;
        }
        var item = makeObject();
        objectFor(item)->setProperty("nodeId", assignment.targetNodeId);
        objectFor(item)->setProperty("kind", assignment.targetKind == GuideCurveTargetKind::EnvelopeCubeComponent
                ? "envelopeCubeComponent" : "trimeshCubeComponent");
        objectFor(item)->setProperty("cubeIndex", assignment.target.cubeIndex);
        objectFor(item)->setProperty("field",
                TrimeshGuideAttachmentTarget::fields()[(size_t) assignment.target.field]);
        assignments.add(std::move(item));
    }
    fields.setProperty("assignments", std::move(assignments));
    return data;
}

bool attachmentRequest(const GraphDocument& document, const var& request) {
    const auto* node = document.graph().findNode(request["nodeId"].toString());
    const var index = request["selectionIndex"];
    return request["nodeId"].isString() && node != nullptr && node->model != nullptr
            && positiveInteger(request["expectedModelRevision"])
            && (uint64_t) (int64) request["expectedModelRevision"] == node->model->revision()
            && (index.isInt() || index.isInt64()) && (int64) index >= 0
            && (int64) index <= std::numeric_limits<int>::max()
            && request["field"].isString();
}

var editResponse(const String& command, const GraphDocument& document,
        const GraphEditResult& result) {
    var response = result.succeeded() ? okResult(command) : failedResult(command,
            result.message.isNotEmpty() ? result.message : "Guide edit rejected");
    var data = makeObject();
    objectFor(data)->setProperty("changed", result.succeeded() && result.changed);
    objectFor(data)->setProperty("documentRevision", (int64) document.revision());
    objectFor(data)->setProperty(command == "detachGuideCurve" ? "nodeId" : "guideId", result.nodeId);
    if (!result.succeeded()) {
        objectFor(data)->setProperty("code", (int) result.code);
        objectFor(data)->setProperty("editIndex", result.editIndex);
    }
    objectFor(response)->setProperty("data", data);
    return response;
}

}

var runGuideCurveCommand(GraphDocument& document,
        GraphCommandDispatcher& commands, const var& request) {
    const String command = request["command"].toString();
    const String guideId = request["guideId"].toString();
    if ((command == "inspectGuideCurve" || command == "editGuideCurve"
                    || command == "assignGuideCurve")
            && (!request["guideId"].isString() || guideId.isEmpty())) {
        return failedResult(command, "Requires a nonempty guideId string");
    }
    if (command == "inspectGuideCurve") {
        const var data = inspectGuide(document, guideId);
        return data.isVoid() ? failedResult(command, "Unknown Guide: " + guideId)
                : okResult(command, data);
    }
    if (!positiveInteger(request["expectedDocumentRevision"])
            || (uint64_t) (int64) request["expectedDocumentRevision"] != document.revision()) {
        return failedResult(command, "Requires the current expectedDocumentRevision");
    }
    if (commands.hasActiveEdit()) {
        return failedResult(command, "Finish the active gesture before editing Guides");
    }
    GraphEditResult result;
    if (command == "editGuideCurve") {
        GuideCurveEdits edits;
        if (!decodePointEdits(request, edits)) {
            return failedResult(command, "Requires expectedModelRevision and 1 to 64 typed point edits");
        }
        result = commands.editGuideCurve(edits);
    } else if (command == "createGuideCurve") {
        result = commands.createGuideCurve();
    } else if (command == "assignGuideCurve" || command == "detachGuideCurve") {
        if (!attachmentRequest(document, request)) {
            return failedResult(command,
                    "Requires nodeId, current expectedModelRevision, selectionIndex and field");
        }
        const String nodeId = request["nodeId"].toString();
        const int index = (int) request["selectionIndex"];
        const String field = request["field"].toString();
        result = command == "assignGuideCurve"
                ? commands.assignGuideCurve(guideId, nodeId, index, field)
                : commands.detachGuideCurve(nodeId, index, field);
    } else {
        return failedResult(command, "Unknown Guide operation");
    }
    return editResponse(command, document, result);
}

}
