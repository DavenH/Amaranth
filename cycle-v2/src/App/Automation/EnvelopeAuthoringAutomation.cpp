#include <algorithm>
#include <cmath>
#include <limits>

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>

#include "App/Automation/EnvelopeAuthoringAutomation.h"
#include "App/Automation/CycleV2AutomationProtocol.h"
#include "Graph/EnvelopeAuthoringEdits.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/MeshDimensionality.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"
#include "Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

using namespace AutomationProtocol;

namespace {

bool integer(const var& value, int64 minimum, int64 maximum = std::numeric_limits<int64>::max()) {
    return (value.isInt() || value.isInt64()) && (int64) value >= minimum && (int64) value <= maximum;
}

bool finite(const var& value) {
    return (value.isInt() || value.isInt64() || value.isDouble())
            && std::isfinite((double) value)
            && std::abs((double) value) <= std::numeric_limits<float>::max();
}

bool onlyFields(const var& value, std::initializer_list<const char*> fields) {
    const auto* object = value.getDynamicObject();
    if (object == nullptr) {
        return false;
    }
    for (const auto& property : object->getProperties()) {
        const auto name = property.name.toString();
        if (std::none_of(fields.begin(), fields.end(), [&](const char* field) { return name == field; })) {
            return false;
        }
    }
    return true;
}

bool decode(const var& request, EnvelopeAuthoringEdits& result) {
    if (!integer(request["expectedDocumentRevision"], 1)
            || !integer(request["expectedModelRevision"], 1)
            || !integer(request["dimensionCount"], 0, 3)) {
        return false;
    }
    const auto links = request["linkedAxes"];
    if (!onlyFields(links, { "yellow", "red", "blue" })
            || !links["yellow"].isBool() || !links["red"].isBool() || !links["blue"].isBool()) {
        return false;
    }
    const auto* edits = request["edits"].getArray();
    if (edits == nullptr || edits->isEmpty() || (size_t) edits->size() > maximumEnvelopeAuthoringEdits) {
        return false;
    }
    result.nodeId = request["nodeId"].toString();
    result.expectedDocumentRevision = (uint64_t) (int64) request["expectedDocumentRevision"];
    result.expectedModelRevision = (uint64_t) (int64) request["expectedModelRevision"];
    result.dimensionCount = (int) request["dimensionCount"];
    result.linkedAxes = { (bool) links["yellow"], (bool) links["red"], (bool) links["blue"] };
    for (const auto& item : *edits) {
        if (!item["op"].isString() || !integer(item["cubeId"], 1)) {
            return false;
        }
        EnvelopeAuthoringEdit edit;
        edit.cubeId = (uint64_t) (int64) item["cubeId"];
        const auto operation = item["op"].toString();
        if (operation == "value" || operation == "guideGain") {
            if (!onlyFields(item, { "op", "cubeId", "corner", "field", "value" })
                    || !item["field"].isString() || !finite(item["value"])) {
                return false;
            }
            edit.field = item["field"].toString();
            const double value = item["value"];
            if (value < 0. || value > (operation == "value" && edit.field == "phase"
                    ? (double) EnvelopeEditCore::maximumPhase : 1.)) {
                return false;
            }
            edit.value = (float) (double) item["value"];
            if (operation == "value") {
                if (!integer(item["corner"], 0, 7)
                        || (edit.field != "phase" && edit.field != "amp" && edit.field != "curve"
                                && edit.field != "yellow" && edit.field != "red" && edit.field != "blue")) {
                    return false;
                }
                edit.corner = (int) item["corner"];
                edit.kind = EnvelopeAuthoringEditKind::Value;
            } else {
                if (item.hasProperty("corner") || (edit.field != "component" && edit.field != "red"
                        && edit.field != "blue" && edit.field != "phase" && edit.field != "amp"
                        && edit.field != "curve")) {
                    return false;
                }
                edit.kind = EnvelopeAuthoringEditKind::GuideGain;
            }
        } else if (operation == "loop" || operation == "sustain") {
            if (!onlyFields(item, { "op", "cubeId", "enabled" }) || !item["enabled"].isBool()) {
                return false;
            }
            edit.kind = operation == "loop" ? EnvelopeAuthoringEditKind::LoopMarker
                    : EnvelopeAuthoringEditKind::SustainMarker;
            edit.enabled = (bool) item["enabled"];
        } else {
            return false;
        }
        result.edits.push_back(std::move(edit));
    }
    return true;
}

var inspect(const GraphDocument& document, const String& nodeId) {
    const auto* node = document.graph().findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::Envelope) {
        return {};
    }
    const auto state = std::dynamic_pointer_cast<const CurveNodeModelState>(node->model);
    if (state == nullptr || state->envelope() == nullptr) {
        return {};
    }
    const auto& model = *state->envelope();
    const auto& mesh = model.getMesh();
    auto data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("nodeId", nodeId);
    object->setProperty("documentRevision", (int64) document.revision());
    object->setProperty("modelRevision", (int64) state->revision());
    object->setProperty("dimensionCount", MeshDimensionality::dimensions(*node).axisCount());
    object->setProperty("purpose", envelopePurposeToString(envelopePurposeFor(*node)));
    object->setProperty("phaseUnits", "Envelope time; 1 is the owning Voice Context duration");
    object->setProperty("phaseMinimum", 0);
    object->setProperty("phaseMaximum", EnvelopeEditCore::maximumPhase);
    object->setProperty("unitFields", "yellow, red, blue, amp, curve and stored Guide gains: [0,1]");
    object->setProperty("guideGainUnits", "Stored gain; .5 is unity, existing cube gain conversion applies");
    object->setProperty("linkScope", "Every edit request supplies all three linkedAxes booleans");
    const auto morph = MeshDimensionality::position(*node);
    auto authored = makeObject();
    objectFor(authored)->setProperty("yellow", (float) morph.time);
    objectFor(authored)->setProperty("red", (float) morph.red);
    objectFor(authored)->setProperty("blue", (float) morph.blue);
    object->setProperty("authoredMorph", authored);
    Array<var> cubes;
    for (size_t index = 0; index < mesh.getCubes().size(); ++index) {
        const auto* cube = mesh.getCubes()[index];
        auto encoded = makeObject();
        objectFor(encoded)->setProperty("cubeId", (int64) model.getCubeIds()[index]);
        objectFor(encoded)->setProperty("cubeIndex", (int) index);
        objectFor(encoded)->setProperty("loop", EnvelopeEditCore::markerState(mesh, *cube, true));
        objectFor(encoded)->setProperty("sustain", EnvelopeEditCore::markerState(mesh, *cube, false));
        Array<var> corners;
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            auto point = makeObject();
            objectFor(point)->setProperty("corner", corner);
            for (const auto* field : { "yellow", "red", "blue", "phase", "amp", "curve" }) {
                objectFor(point)->setProperty(field,
                        cube->getVertex(corner)->values[EnvelopeEditCore::fieldDimension(field)]);
            }
            corners.add(std::move(point));
        }
        objectFor(encoded)->setProperty("corners", corners);
        auto gains = makeObject();
        auto assignments = makeObject();
        for (const String field : { "component", "red", "blue", "phase", "amp", "curve" }) {
            const int dimension = EnvelopeEditCore::fieldDimension(field);
            objectFor(gains)->setProperty(field, cube->guideCurveGainAt(dimension));
            const GuideCurveField guideField = field == "component" ? GuideCurveField::Time
                    : field == "red" ? GuideCurveField::Red
                    : field == "blue" ? GuideCurveField::Blue
                    : field == "phase" ? GuideCurveField::Phase
                    : field == "amp" ? GuideCurveField::Amplitude : GuideCurveField::Curve;
            const auto* assignment = document.graph().guideAssignmentForTarget(nodeId, { (int) index, guideField });
            objectFor(assignments)->setProperty(field, assignment != nullptr ? assignment->guideId : String());
        }
        objectFor(encoded)->setProperty("guideGains", gains);
        objectFor(encoded)->setProperty("guides", assignments);
        cubes.add(std::move(encoded));
    }
    object->setProperty("cubes", cubes);
    return data;
}

}

var runEnvelopeAuthoringCommand(GraphDocument& document,
        GraphCommandDispatcher& commands, const var& request) {
    const String command = request["command"].toString();
    if (!request["nodeId"].isString() || request["nodeId"].toString().isEmpty()) {
        return failedResult(command, "Requires a nonempty nodeId");
    }
    if (command == "inspectEnvelope") {
        const auto data = inspect(document, request["nodeId"].toString());
        return data.isVoid() ? failedResult(command, "Envelope does not exist") : okResult(command, data);
    }
    EnvelopeAuthoringEdits edits;
    if (command != "editEnvelope" || !decode(request, edits)) {
        return failedResult(command,
                "Requires revisions, dimensionCount, all linkedAxes booleans and 1 to 64 typed Envelope edits");
    }
    const auto result = commands.editEnvelope(edits);
    auto response = result.succeeded() ? okResult(command)
            : failedResult(command, result.message.isNotEmpty() ? result.message : "Envelope edit rejected");
    auto data = result.succeeded() ? inspect(document, edits.nodeId) : makeObject();
    objectFor(data)->setProperty("changed", result.succeeded() && result.changed);
    objectFor(data)->setProperty("documentRevision", (int64) document.revision());
    if (!result.succeeded()) {
        objectFor(data)->setProperty("code", (int) result.code);
        objectFor(data)->setProperty("editIndex", result.editIndex);
    }
    objectFor(response)->setProperty("data", data);
    return response;
}

}
