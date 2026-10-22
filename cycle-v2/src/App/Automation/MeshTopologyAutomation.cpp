#include <cmath>
#include <limits>

#include <Curve/Mesh/Mesh.h>

#include "App/Automation/MeshTopologyAutomation.h"
#include "App/Automation/CycleV2AutomationProtocol.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/MeshTopologyEdits.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

namespace CycleV2 {
using namespace AutomationProtocol;

namespace {

bool integer(const var& value, int64 minimum, int64 maximum) {
    return (value.isInt() || value.isInt64()) && (int64) value >= minimum && (int64) value <= maximum;
}

bool coordinate(const var& item, const char* field, float& target, double maximum) {
    const var value = item[field];
    if ((!value.isInt() && !value.isInt64() && !value.isDouble()) || !std::isfinite((double) value)
            || (double) value < 0.0 || (double) value > maximum) {
        return false;
    }
    target = (float) (double) value;
    return std::isfinite(target);
}

}

var runMeshTopologyCommand(GraphDocument& document, GraphCommandDispatcher& commands, const var& request) {
    constexpr auto maximum = std::numeric_limits<int64>::max();
    const String command = request["command"].toString();
    const auto* list = request["edits"].getArray();
    if (!request["nodeId"].isString() || request["nodeId"].toString().isEmpty()
            || !integer(request["expectedDocumentRevision"], 1, maximum)
            || !integer(request["expectedModelRevision"], 1, maximum)
            || list == nullptr || list->isEmpty() || (size_t) list->size() > maximumMeshTopologyEdits) {
        return failedResult(command, "Requires nodeId, current document/model revisions and 1 to 16 edits");
    }
    MeshTopologyEdits decoded;
    decoded.nodeId = request["nodeId"].toString();
    decoded.expectedDocumentRevision = (uint64_t) (int64) request["expectedDocumentRevision"];
    decoded.expectedModelRevision = (uint64_t) (int64) request["expectedModelRevision"];
    const auto* target = document.graph().findNode(decoded.nodeId);
    const double maximumPhase = target != nullptr && target->kind == NodeKind::Envelope
            ? EnvelopeEditCore::maximumPhase : 1.0;
    for (const auto& item : *list) {
        MeshTopologyEdit edit;
        const auto op = item["op"].toString();
        const auto* fields = item.getDynamicObject();
        if (fields == nullptr || !item["op"].isString() || (op != "insert" && op != "remove")) {
            return failedResult(command, "Topology operations are insert and remove");
        }
        for (const auto& property : fields->getProperties()) {
            const auto name = property.name.toString();
            if (name != "op" && name != "cubeIndex" && name != "cubeId"
                    && (op != "insert" || (name != "phase" && name != "amp" && name != "curve"))) {
                return failedResult(command, "Unexpected topology field: " + name);
            }
        }
        edit.kind = op == "insert" ? MeshTopologyEditKind::Insert : MeshTopologyEditKind::Remove;
        if (item.hasProperty("cubeIndex")) {
            if (!integer(item["cubeIndex"], 0, std::numeric_limits<int>::max())) {
                return failedResult(command, "cubeIndex must be a nonnegative integer");
            }
            edit.cubeIndex = (int) item["cubeIndex"];
        }
        if (item.hasProperty("cubeId")) {
            if (!integer(item["cubeId"], 1, maximum)) {
                return failedResult(command, "cubeId must be a positive integer");
            }
            edit.cubeId = (uint64_t) (int64) item["cubeId"];
        }
        if (edit.kind == MeshTopologyEditKind::Insert
                && (!coordinate(item, "phase", edit.phase, maximumPhase) || !coordinate(item, "amp", edit.amp, 1.0)
                        || !coordinate(item, "curve", edit.curve, 1.0))) {
            return failedResult(command, "Insertion requires canonical phase, amp and curve values");
        }
        decoded.edits.push_back(edit);
    }
    const auto result = commands.editMeshTopology(decoded);
    var data = makeObject();
    objectFor(data)->setProperty("changed", result.succeeded() && result.changed);
    objectFor(data)->setProperty("documentRevision", (int64) document.revision());
    const auto* node = document.graph().findNode(decoded.nodeId);
    objectFor(data)->setProperty("nodeId", decoded.nodeId);
    objectFor(data)->setProperty("modelRevision", (int64) (node != nullptr && node->model != nullptr ? node->model->revision() : 0));
    objectFor(data)->setProperty("reinspect", node != nullptr && node->kind == NodeKind::Envelope ? "inspectEnvelope" : "inspectTrimesh");
    objectFor(data)->setProperty("editIndex", result.editIndex);
    if (result.succeeded() && node != nullptr) {
        Array<var> addresses;
        const auto envelope = std::dynamic_pointer_cast<const CurveNodeModelState>(node->model);
        const auto trimesh = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model);
        const auto* ids = envelope != nullptr && envelope->envelope() != nullptr
                ? &envelope->envelope()->getCubeIds() : nullptr;
        const int count = ids != nullptr ? (int) ids->size()
                : trimesh != nullptr ? trimesh->mesh().getNumCubes() : 0;
        for (int index = 0; index < count; ++index) {
            auto address = makeObject();
            objectFor(address)->setProperty("cubeIndex", index);
            if (ids != nullptr) {
                objectFor(address)->setProperty("cubeId", (int64) (*ids)[(size_t) index]);
            }
            addresses.add(std::move(address));
        }
        objectFor(data)->setProperty("cubeAddresses", addresses);
    }
    auto response = result.succeeded() ? okResult(command, data) : failedResult(command, result.message);
    objectFor(response)->setProperty("data", data);
    return response;
}

}
