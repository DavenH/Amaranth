#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

#include "App/TrimeshAuthoringAutomation.h"
#include "App/CycleV2AutomationProtocol.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/MeshDimensionality.h"
#include "Graph/TrimeshAuthoringEdits.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"

namespace CycleV2 {

using namespace AutomationProtocol;

namespace {

constexpr std::array<int, 6> coordinates {
        Vertex::Time, Vertex::Red, Vertex::Blue, Vertex::Phase, Vertex::Amp, Vertex::Curve
};

bool positiveRevision(const var& value) {
    return (value.isInt() || value.isInt64()) && (int64) value > 0;
}

bool readIndex(const var& value, int& index) {
    if ((!value.isInt() && !value.isInt64()) || (int64) value < 0
            || (int64) value > std::numeric_limits<int>::max()) {
        return false;
    }
    index = (int) value;
    return true;
}

bool decodeEdits(const var& request, TrimeshAuthoringEdits& decoded) {
    const auto* edits = request["edits"].getArray();
    if (!positiveRevision(request["expectedDocumentRevision"])
            || !positiveRevision(request["expectedModelRevision"]) || edits == nullptr
            || edits->isEmpty() || (size_t) edits->size() > maximumTrimeshAuthoringEdits) {
        return false;
    }
    decoded.nodeId = request["nodeId"].toString();
    decoded.expectedDocumentRevision = (uint64_t) (int64) request["expectedDocumentRevision"];
    decoded.expectedModelRevision = (uint64_t) (int64) request["expectedModelRevision"];
    for (const auto& entry : *edits) {
        const var value = entry["value"];
        if (entry.getDynamicObject() == nullptr || !entry["op"].isString()
                || (!value.isInt() && !value.isInt64() && !value.isDouble())
                || !std::isfinite((double) value) || (double) value < 0. || (double) value > 1.) {
            return false;
        }
        TrimeshAuthoringEdit edit;
        edit.value = (float) (double) value;
        const String operation = entry["op"].toString();
        for (const auto& property : entry.getDynamicObject()->getProperties()) {
            const auto name = property.name.toString();
            const bool address = operation == "cubeCurve" ? name == "cubeIndex"
                    : name == "vertexIndex" || name == "field";
            if (name != "op" && name != "value" && !address) {
                return false;
            }
        }
        if (operation == "cubeCurve") {
            edit.kind = TrimeshAuthoringEditKind::CubeCurve;
            if (!readIndex(entry["cubeIndex"], edit.cubeIndex)
                    || entry.hasProperty("vertexIndex") || entry.hasProperty("field")) {
                return false;
            }
        } else {
            if ((operation != "vertex" && operation != "guideGain")
                    || !readIndex(entry["vertexIndex"], edit.vertexIndex)
                    || entry.hasProperty("cubeIndex") || !entry["field"].isString()) {
                return false;
            }
            edit.kind = operation == "vertex" ? TrimeshAuthoringEditKind::Vertex
                    : TrimeshAuthoringEditKind::GuideGain;
            edit.field = entry["field"].toString();
        }
        decoded.edits.push_back(std::move(edit));
    }
    return true;
}

var inspectFields(const MorphDimensions& dimensions) {
    Array<var> result;
    const auto& names = TrimeshGuideAttachmentTarget::fields();
    for (size_t index = 0; index < names.size(); ++index) {
        var field = makeObject();
        auto& properties = *objectFor(field);
        properties.setProperty("field", names[index]);
        properties.setProperty("minimum", 0.);
        properties.setProperty("maximum", 1.);
        properties.setProperty("units", "canonical normalized coordinate");
        properties.setProperty("vertexEditable", names[index] != "curve"
                && (index >= 3 || dimensions.active(coordinates[index])));
        result.add(std::move(field));
    }
    return result;
}

var inspectGeometry(const NodeGraph& graph, const Node& node, const Mesh& mesh) {
    std::unordered_map<const Vertex*, int> vertexIndices;
    std::unordered_map<const VertCube*, int> cubeIndices;
    for (size_t index = 0; index < mesh.getCubes().size(); ++index) {
        cubeIndices.emplace(mesh.getCubes()[index], (int) index);
    }
    Array<var> vertices;
    const auto& names = TrimeshGuideAttachmentTarget::fields();
    for (size_t index = 0; index < mesh.getVerts().size(); ++index) {
        const auto* vertex = mesh.getVerts()[index];
        vertexIndices.emplace(vertex, (int) index);
        var item = makeObject();
        var values = makeObject();
        var gains = makeObject();
        for (size_t field = 0; field < names.size(); ++field) {
            objectFor(values)->setProperty(names[field], vertex->values[coordinates[field]]);
            objectFor(gains)->setProperty(names[field], TrimeshVertexEditCore::guideGain(
                    mesh, (int) index, "guideGain." + names[field]));
        }
        Array<var> owners;
        for (const auto* owner : vertex->owners) {
            const auto found = cubeIndices.find(owner);
            if (found != cubeIndices.end()) {
                owners.add(found->second);
            }
        }
        objectFor(item)->setProperty("vertexIndex", (int) index);
        objectFor(item)->setProperty("values", values);
        objectFor(item)->setProperty("guideGains", gains);
        objectFor(item)->setProperty("ownerCubeIndices", owners);
        vertices.add(std::move(item));
    }
    Array<var> cubes;
    for (size_t index = 0; index < mesh.getCubes().size(); ++index) {
        const auto* cube = mesh.getCubes()[index];
        var item = makeObject();
        var gains = makeObject();
        var assignments = makeObject();
        Array<var> corners;
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            corners.add(vertexIndices.at(cube->getVertex(corner)));
        }
        for (size_t field = 0; field < names.size(); ++field) {
            objectFor(gains)->setProperty(names[field], cube->guideCurveGainAt(coordinates[field]));
            const auto* guide = graph.guideAssignmentForTarget(node.id,
                    { (int) index, TrimeshGuideAttachmentTarget::guideField(names[field]) });
            if (guide != nullptr) {
                objectFor(assignments)->setProperty(names[field], guide->guideId);
            }
        }
        objectFor(item)->setProperty("cubeIndex", (int) index);
        objectFor(item)->setProperty("vertexIndices", corners);
        objectFor(item)->setProperty("guideGains", gains);
        objectFor(item)->setProperty("guideAssignments", assignments);
        cubes.add(std::move(item));
    }
    var result = makeObject();
    objectFor(result)->setProperty("vertices", vertices);
    objectFor(result)->setProperty("cubes", cubes);
    return result;
}

var inspectTrimesh(const GraphDocument& document, const Node& node, const Mesh& mesh) {
    var result = inspectGeometry(document.graph(), node, mesh);
    auto& fields = *objectFor(result);
    fields.setProperty("nodeId", node.id);
    fields.setProperty("documentRevision", (int64) document.revision());
    fields.setProperty("modelRevision", (int64) node.model->revision());
    fields.setProperty("dimensionCount", MeshDimensionality::dimensions(node).axisCount());
    fields.setProperty("addressScope", "vertexIndex and cubeIndex are ordinals at modelRevision");
    fields.setProperty("fields", inspectFields(MeshDimensionality::dimensions(node)));
    fields.setProperty("guideGainScope", "incident cubes; mean strength with clamped per-owner delta");
    fields.setProperty("guideGainUnits", "normalized 0..1; 0.5 is 0 dB, endpoints -30/+30 dB");
    fields.setProperty("curveScope", "cubeCurve changes one cube's mean corner sharpness");
    fields.setProperty("selectedVertexIndex", node.editorState.getProperty("selectedVertexId", -1));
    return result;
}

var editResponse(const GraphDocument& document, const GraphEditResult& result) {
    var response = result.succeeded() ? okResult("editTrimesh")
            : failedResult("editTrimesh", result.message);
    var data = makeObject();
    objectFor(data)->setProperty("nodeId", result.nodeId);
    objectFor(data)->setProperty("documentRevision", (int64) document.revision());
    objectFor(data)->setProperty("changed", result.succeeded() && result.changed);
    const auto* node = document.graph().findNode(result.nodeId);
    if (node != nullptr && node->model != nullptr) {
        objectFor(data)->setProperty("modelRevision", (int64) node->model->revision());
    }
    if (!result.succeeded()) {
        objectFor(data)->setProperty("code", (int) result.code);
        objectFor(data)->setProperty("editIndex", result.editIndex);
    }
    objectFor(response)->setProperty("data", data);
    return response;
}

}

var runTrimeshAuthoringCommand(GraphDocument& document,
        GraphCommandDispatcher& commands, const var& request) {
    const String command = request["command"].toString();
    if (!request["nodeId"].isString() || request["nodeId"].toString().isEmpty()) {
        return failedResult(command, "Requires a nonempty nodeId string");
    }
    if (command == "inspectTrimesh") {
        const auto* node = document.graph().findNode(request["nodeId"].toString());
        const auto state = node != nullptr && node->kind == NodeKind::TrilinearMesh
                ? std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model) : nullptr;
        return state != nullptr ? okResult(command, inspectTrimesh(document, *node, state->mesh()))
                : failedResult(command, "Node is not a Trimesh with a model");
    }
    TrimeshAuthoringEdits edits;
    if (command != "editTrimesh" || !decodeEdits(request, edits)) {
        return failedResult(command, "Requires current revisions and 1 to 64 typed Trimesh edits");
    }
    return editResponse(document, commands.editTrimesh(edits));
}

}
