#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/GraphInvalidation.h"

#include <algorithm>

using namespace CycleV2;

namespace {

bool containsNode(const std::vector<String>& nodes, const String& nodeId) {
    return std::find(nodes.begin(), nodes.end(), nodeId) != nodes.end();
}

}

TEST_CASE("Graph invalidation marks downstream signal dependents", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "fft",
            GraphChangeKind::NodeParameters);

    REQUIRE_FALSE(result.requiresRecompile);
    REQUIRE(containsNode(result.audioNodes, "fft"));
    REQUIRE(containsNode(result.audioNodes, "addMag"));
    REQUIRE(containsNode(result.audioNodes, "addPhase"));
    REQUIRE(containsNode(result.audioNodes, "ifft"));
    REQUIRE(containsNode(result.audioNodes, "out"));
    REQUIRE_FALSE(containsNode(result.audioNodes, "waveMesh"));
    REQUIRE(result.previewNodes == result.audioNodes);
}

TEST_CASE("Graph invalidation follows attachment dependencies", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "scratchEnv",
            GraphChangeKind::NodeContent);

    REQUIRE(containsNode(result.audioNodes, "scratchEnv"));
    REQUIRE(containsNode(result.audioNodes, "waveMesh"));
    REQUIRE(containsNode(result.audioNodes, "magMesh"));
    REQUIRE(containsNode(result.audioNodes, "addMag"));
    REQUIRE(containsNode(result.audioNodes, "out"));
}

TEST_CASE("Graph invalidation flags schema changes for recompilation", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "waveMesh",
            GraphChangeKind::PortSchema);

    REQUIRE(result.requiresRecompile);
    REQUIRE(containsNode(result.audioNodes, "waveMesh"));
}
