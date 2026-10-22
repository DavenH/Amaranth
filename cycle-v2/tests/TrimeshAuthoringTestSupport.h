#pragma once

#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/MorphDimensions.h>

#include "Graph/GraphNodeFactory.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

namespace CycleV2::TestSupport {

inline NodeGraph authoredTrimeshGraph(int dimensions = 3, int unrelatedNodes = 0) {
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("TrimeshAuthoringFixture");
    MorphDimensions::project(*mesh, MorphDimensions::trimesh(dimensions), { 0.4f, 0.3f, 0.7f });
    node.model = TrimeshNodeModelState::takeOwnership(std::move(mesh), 1);
    node.editorState = JSON::parse(R"({"selectedVertexId":8,"sentinel":"preserve"})");
    for (auto& parameter : node.parameters) {
        if (parameter.id == "dimensionCount") {
            parameter.value = String(dimensions);
        }
    }
    graph.addNode(std::move(node));
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.name = "Reed grain";
    guide.shortLabel = "G1";
    guide.noiseSeed = 123;
    guide.model = createDefaultGuideCurveModel();
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({ "guide1", "mesh", { 2, GuideCurveField::Amplitude } }));
    for (int index = 0; index < unrelatedNodes; ++index) {
        graph.addNode(GraphNodeFactory().createNode(
                NodeKind::TrilinearMesh, "unrelated" + String(index), {}));
    }
    return graph;
}

}
