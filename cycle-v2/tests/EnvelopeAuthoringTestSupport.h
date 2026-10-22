#pragma once

#include <catch2/catch_test_macros.hpp>

#include <Curve/Curve.h>
#include <Curve/Mesh/MorphDimensions.h>
#include <Curve/Mesh/VertCube.h>

#include "Graph/EnvelopeAuthoringEdits.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2::TestSupport {

struct EnvelopeAuthoringCurveTables {
    EnvelopeAuthoringCurveTables() { Curve::calcTable(); }
    ~EnvelopeAuthoringCurveTables() { Curve::deleteTable(); }
};

inline const EnvelopeNodeModel& envelopeFor(const GraphDocument& document) {
    const auto state = std::dynamic_pointer_cast<const CurveNodeModelState>(
            document.graph().findNode("env")->model);
    REQUIRE(state != nullptr);
    REQUIRE(state->envelope() != nullptr);
    return *state->envelope();
}

inline NodeGraph authoredEnvelopeGraph(int dimensions = 3, int unrelated = 0) {
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    EnvelopeNodeModel model;
    auto& mesh = model.getMesh();
    for (size_t index = 0; index < mesh.getCubes().size(); ++index) {
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            auto* vertex = mesh.getCubes()[index]->getVertex(corner);
            vertex->values[Vertex::Time] = (corner & 4) != 0 ? 1.f : 0.f;
            vertex->values[Vertex::Red] = (corner & 2) != 0 ? 1.f : 0.f;
            vertex->values[Vertex::Blue] = (corner & 1) != 0 ? 1.f : 0.f;
            vertex->values[Vertex::Amp] = 0.2f + 0.05f * (float) corner;
            vertex->values[Vertex::Curve] = index == 3 ? 1.f : 0.4f;
        }
    }
    MorphDimensions::project(mesh, MorphDimensions(dimensions), { 0.4f, 0.3f, 0.7f });
    REQUIRE(model.synchronizeFromMesh(mesh.getCubes()[1]));
    auto editor = std::make_unique<DynamicObject>();
    editor->setProperty("selectedCubeId", (int64) model.getCubeIds()[1]);
    editor->setProperty("sentinel", "preserve");
    node.editorState = var(editor.release());
    node.model = CurveNodeModelState::copyOf(model, 0.3f, 0.7f, 1, node.editorState);
    for (auto& parameter : node.parameters) {
        if (parameter.id == "dimensionCount") {
            parameter.value = String(dimensions);
        } else if (parameter.id == "yellow") {
            parameter.value = "0.4";
        } else if (parameter.id == "red") {
            parameter.value = "0.3";
        } else if (parameter.id == "blue") {
            parameter.value = "0.7";
        }
    }
    graph.addNode(std::move(node));
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.shortLabel = "G1";
    guide.name = "Envelope grain";
    guide.noiseSeed = 123;
    guide.model = createDefaultGuideCurveModel();
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({ "guide1", "env", { 2, GuideCurveField::Amplitude },
            GuideCurveTargetKind::EnvelopeCubeComponent }));
    for (int index = 0; index < unrelated; ++index) {
        graph.addNode(GraphNodeFactory().createNode(
                NodeKind::Envelope, "unrelated" + String(index), {}));
    }
    return graph;
}

inline EnvelopeAuthoringEdits envelopeRequest(const GraphDocument& document,
        std::vector<EnvelopeAuthoringEdit> edits,
        std::array<bool, 3> links = { true, true, true }) {
    const auto* node = document.graph().findNode("env");
    const auto dimensions = NodeParameterMap(*node).intValue("dimensionCount", 0);
    return { "env", document.revision(), node->model->revision(), dimensions, links, std::move(edits) };
}

}
