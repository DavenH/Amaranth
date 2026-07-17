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
            ParameterImpact::DspConfiguration);

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
            ParameterImpact::DspConfiguration | ParameterImpact::Preview);

    REQUIRE(containsNode(result.audioNodes, "scratchEnv"));
    REQUIRE(containsNode(result.audioNodes, "waveMesh"));
    REQUIRE(containsNode(result.audioNodes, "magMesh"));
    REQUIRE(containsNode(result.audioNodes, "addMag"));
    REQUIRE(containsNode(result.audioNodes, "out"));
}

TEST_CASE("Graph invalidation limits preview impacts to preview dependents", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "waveMesh",
            ParameterImpact::Preview);

    REQUIRE_FALSE(result.requiresRecompile);
    REQUIRE(result.audioNodes.empty());
    REQUIRE(containsNode(result.previewNodes, "waveMesh"));
    REQUIRE(containsNode(result.previewNodes, "out"));
}

TEST_CASE("Graph invalidation flags graph semantic impacts for recompilation", "[cycle-v2][runtime]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "waveMesh",
            ParameterImpact::GraphSemantics);

    REQUIRE(result.requiresRecompile);
    REQUIRE(containsNode(result.audioNodes, "waveMesh"));
}

TEST_CASE("Graph compiler publishes one combined signal and attachment dependency index",
        "[cycle-v2][runtime][complexity]") {
    const auto compileResult = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compileResult.succeeded());
    const auto& index = compileResult.plan.dependencyIndex;

    REQUIRE(index.nodeIds == compileResult.plan.nodeOrder);
    REQUIRE(index.dependents.size() == index.nodeIds.size());

    const auto result = GraphInvalidation().invalidateFrom(
            compileResult.plan,
            "scratchEnv",
            ParameterImpact::DspConfiguration);
    REQUIRE(result.visitedNodeCount == result.audioNodes.size());
    REQUIRE(result.inspectedDependencyCount > 0);
}

TEST_CASE("Graph invalidation inspects each reachable dependency once",
        "[cycle-v2][runtime][complexity]") {
    GraphExecutionPlan plan;
    plan.dependencyIndex.nodeIds = { "root", "left", "right", "join", "tail" };
    plan.dependencyIndex.dependents = {
        { 1, 2 },
        { 3 },
        { 3 },
        { 4 },
        {}
    };

    const auto result = GraphInvalidation().invalidateFrom(
            plan,
            "root",
            ParameterImpact::Preview);

    REQUIRE(result.previewNodes == plan.dependencyIndex.nodeIds);
    REQUIRE(result.audioNodes.empty());
    REQUIRE(result.visitedNodeCount == 5);
    REQUIRE(result.inspectedDependencyCount == 5);
}

TEST_CASE("Graph invalidation count scales linearly with a dependency chain",
        "[cycle-v2][runtime][complexity]") {
    for (const int nodeCount : { 16, 32, 64 }) {
        GraphExecutionPlan plan;
        plan.dependencyIndex.nodeIds.reserve((size_t) nodeCount);
        plan.dependencyIndex.dependents.resize((size_t) nodeCount);
        for (int i = 0; i < nodeCount; ++i) {
            plan.dependencyIndex.nodeIds.push_back("node" + String(i));
            if (i + 1 < nodeCount) {
                plan.dependencyIndex.dependents[(size_t) i].push_back(i + 1);
            }
        }

        const auto result = GraphInvalidation().invalidateFrom(
                plan,
                "node0",
                ParameterImpact::DspConfiguration);
        REQUIRE(result.visitedNodeCount == (size_t) nodeCount);
        REQUIRE(result.inspectedDependencyCount == (size_t) nodeCount - 1);
        REQUIRE(result.audioNodes.size() == (size_t) nodeCount);
        REQUIRE(result.previewNodes == result.audioNodes);
    }
}
