#include <limits>

#include "Graph/GraphCommandDispatcher.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/ModelAuthoringValidation.h"

namespace CycleV2 {

namespace {

GraphEditResult rejected(const GuideCurveEdits& request, GraphEditCode code,
        const String& message, int editIndex = -1) {
    GraphEditResult result { code, request.guideId, {} };
    result.changed = false;
    result.message = message;
    result.editIndex = editIndex;
    return result;
}

CurveEditResult editPoint(FlatCurveModel& model, const GuideCurvePointEdit& edit) {
    const bool hasFields = edit.x.has_value() || edit.y.has_value() || edit.curve.has_value();
    switch (edit.kind) {
        case GuideCurvePointEditKind::Insert:
            if (edit.vertexId != 0 || !edit.x.has_value() || !edit.y.has_value()
                    || model.getVertices().size() >= ModelAuthoringValidation::maximumVertices) {
                return {};
            }
            return model.insertVertex({ *edit.x, *edit.y }, edit.curve.value_or(0.f));
        case GuideCurvePointEditKind::Remove:
            return edit.vertexId != 0 && !hasFields ? model.removeVertex(edit.vertexId)
                    : CurveEditResult {};
        case GuideCurvePointEditKind::Set:
            break;
        default:
            return {};
    }
    if (edit.vertexId == 0 || !hasFields) {
        return {};
    }
    const auto point = model.pointForMeshVertex(model.vertexForIdentity(edit.vertexId));
    if (!point.has_value()) {
        return { CurveEditCode::MissingIdentity, edit.vertexId };
    }
    auto result = model.moveVertex(edit.vertexId,
            { edit.x.value_or(point->x), edit.y.value_or(point->y) });
    if (result.succeeded() && edit.curve.has_value()) {
        result = model.setCurve(edit.vertexId, *edit.curve);
    }
    return result;
}

String storedControl(float value) {
    return String::toDecimalStringWithSignificantFigures(
            value, std::numeric_limits<float>::max_digits10);
}

}

GraphEditResult GraphCommandDispatcher::editGuideCurve(const GuideCurveEdits& request) {
    if (hasActiveEdit()) {
        return rejected(request, GraphEditCode::ValidationRejected,
                "An edit gesture is already active");
    }
    if (request.expectedDocumentRevision != document.revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Document revision has changed");
    }
    if (request.edits.empty() || request.edits.size() > maximumGuideCurvePointEdits) {
        return rejected(request, GraphEditCode::ValidationRejected,
                "Requires 1 to 64 Guide point edits");
    }
    const auto* guide = document.graph().findGuideCurve(request.guideId);
    if (guide == nullptr) {
        return rejected(request, GraphEditCode::MissingNode, "Guide does not exist");
    }
    const auto state = std::dynamic_pointer_cast<const CurveNodeModelState>(guide->model);
    if (state == nullptr || state->flatCurve() == nullptr) {
        return rejected(request, GraphEditCode::InvalidTypedSnapshot,
                "Guide requires a flat curve model");
    }
    if (request.expectedModelRevision != state->revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Guide model revision has changed");
    }
    FlatCurveModel model("CycleV2GuideCurve");
    if (!model.copyFrom(*state->flatCurve())) {
        return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Guide model is invalid");
    }
    for (size_t index = 0; index < request.edits.size(); ++index) {
        if (!editPoint(model, request.edits[index]).succeeded()) {
            return rejected(request, GraphEditCode::InvalidParameterValue,
                    "Invalid Guide point edit", (int) index);
        }
    }
    if (model.getVertices() == state->flatCurve()->getVertices()) {
        GraphEditResult result { GraphEditCode::Connected, request.guideId, {} };
        result.changed = false;
        return result;
    }
    if (state->revision() == std::numeric_limits<uint64_t>::max()) {
        return rejected(request, GraphEditCode::ConflictingRevision,
                "Guide model revision cannot advance");
    }
    return publishGuideCurveState({ request.guideId, guide->revision,
            CurveNodeModelState::copyOf(model, state->revision() + 1, state->editorJSON()),
            { { "enabled", {}, guide->enabled ? "1" : "0" },
              { "noise", {}, storedControl(guide->noise) },
              { "dcOffset", {}, storedControl(guide->dcOffset) },
              { "phase", {}, storedControl(guide->phase) } } });
}

}
