#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodeUpdateGraph.h"

#include <algorithm>

using namespace CycleV2;

namespace {

bool containsNode(const std::vector<String>& nodes, const String& nodeId) {
    return std::find(nodes.begin(), nodes.end(), nodeId) != nodes.end();
}

}

TEST_CASE("Node update graph combines Trimesh local and downstream morph invalidation", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = NodeUpdateGraph().invalidateTrimeshChange(
            compileResult.plan,
            "waveMesh",
            {
                    TrimeshChangeKind::Morph,
                    true,
                    false,
                    false,
                    false,
                    Vertex::Time
            });

    REQUIRE_FALSE(result.requiresRecompile());
    REQUIRE(result.dirtiesAudio());
    REQUIRE(result.dirtiesPreview());
    REQUIRE(containsNode(result.graph.audioNodes, "waveMesh"));
    REQUIRE(containsNode(result.graph.audioNodes, "ifft"));
    REQUIRE(containsNode(result.graph.audioNodes, "out"));
    REQUIRE(result.trimesh.rebuildNodeData);
    REQUIRE(result.trimesh.refresh2DPanel);
    REQUIRE_FALSE(result.trimesh.refresh3DGeometry);
}

TEST_CASE("Node update graph combines Trimesh mesh edits with panel and graph dirtiness", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = NodeUpdateGraph().invalidateTrimeshChange(
            compileResult.plan,
            "magMesh",
            {
                    TrimeshChangeKind::MeshEdit,
                    false,
                    false,
                    false,
                    false,
                    Vertex::Time
            });

    REQUIRE_FALSE(result.requiresRecompile());
    REQUIRE(containsNode(result.graph.previewNodes, "magMesh"));
    REQUIRE(containsNode(result.graph.previewNodes, "addMag"));
    REQUIRE(containsNode(result.graph.previewNodes, "out"));
    REQUIRE(result.trimesh.updateRasterizer);
    REQUIRE(result.trimesh.refresh2DPanel);
    REQUIRE(result.trimesh.refresh3DGeometry);
    REQUIRE(result.trimesh.dirtyCompactPreview);
}

TEST_CASE("Node update graph flags Trimesh render profile changes as preview-only locally", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = NodeUpdateGraph().invalidateTrimeshChange(
            compileResult.plan,
            "magMesh",
            {
                    TrimeshChangeKind::RenderProfile,
                    false,
                    false,
                    false,
                    false,
                    Vertex::Time
            });

    REQUIRE_FALSE(result.requiresRecompile());
    REQUIRE(result.dirtiesPreview());
    REQUIRE_FALSE(result.dirtiesAudio());
    REQUIRE_FALSE(result.trimesh.updateRasterizer);
    REQUIRE_FALSE(result.trimesh.refresh2DPanel);
    REQUIRE_FALSE(result.trimesh.refresh3DGeometry);
    REQUIRE(result.trimesh.dirtyRenderProfile);
}

TEST_CASE("Node update graph keeps Trimesh selected-control changes local", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = NodeUpdateGraph().invalidateTrimeshChange(
            compileResult.plan,
            "waveMesh",
            {
                    TrimeshChangeKind::SelectedControl,
                    false,
                    false,
                    false,
                    false,
                    Vertex::Time
            });

    REQUIRE_FALSE(result.requiresRecompile());
    REQUIRE_FALSE(result.dirtiesAudio());
    REQUIRE_FALSE(result.dirtiesPreview());
    REQUIRE_FALSE(result.trimesh.rebuildNodeData);
    REQUIRE_FALSE(result.trimesh.updateRasterizer);
    REQUIRE(result.trimesh.dirtySelectedControl);
}

TEST_CASE("Node update graph composes Trimesh panel context refresh decisions", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = NodeUpdateGraph().invalidateTrimeshChange(
            compileResult.plan,
            "waveMesh",
            {
                    TrimeshChangeKind::Morph,
                    true,
                    false,
                    false,
                    false,
                    Vertex::Time,
                    true,
                    true
            });

    REQUIRE_FALSE(result.requiresRecompile());
    REQUIRE(result.dirtiesPreview());
    REQUIRE(result.trimesh.rebuildNodeData);
    REQUIRE(result.trimesh.updateRasterizer);
    REQUIRE(result.trimesh.refresh2DPanel);
    REQUIRE(result.trimesh.refresh3DGeometry);
    REQUIRE(result.trimesh.dirtyCompactPreview);
    REQUIRE(result.trimesh.dirtyRenderProfile);
}
