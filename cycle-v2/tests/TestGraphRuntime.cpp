#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/GraphRuntime.h"

#include <algorithm>

using namespace CycleV2;

namespace {

const RuntimeNodeTrace& findTraceNode(const RuntimeProcessTrace& trace, const String& nodeId) {
    const auto found = std::find_if(
            trace.nodes.begin(),
            trace.nodes.end(),
            [&](const RuntimeNodeTrace& node) {
                return node.nodeId == nodeId;
            });

    REQUIRE(found != trace.nodes.end());
    return *found;
}

}

TEST_CASE("Runtime traces compiled graph execution", "[cycle-v2][runtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto trace = GraphRuntime().process(graph, compileResult.plan);

    REQUIRE(trace.nodes.size() == compileResult.plan.nodeOrder.size());
    REQUIRE(trace.nodes.front().nodeId == compileResult.plan.nodeOrder.front());
    REQUIRE(trace.nodes.back().nodeId == "out");
    REQUIRE(findTraceNode(trace, "voice").kind == NodeKind::VoiceContext);
    REQUIRE(findTraceNode(trace, "voice").audioRole == AudioModuleRole::VoiceContext);
    REQUIRE(findTraceNode(trace, "voice").signalInputs.empty());
    REQUIRE(findTraceNode(trace, "waveMesh").kind == NodeKind::TrilinearMesh);
    REQUIRE(findTraceNode(trace, "waveMesh").audioRole == AudioModuleRole::MeshSource);
    REQUIRE(findTraceNode(trace, "waveMesh").previewRole == PreviewModuleRole::MeshSurface);
    REQUIRE(findTraceNode(trace, "waveMesh").cycle1AdapterBacked);
    REQUIRE(findTraceNode(trace, "waveMesh").signalInputs.size() == 1);
    REQUIRE(findTraceNode(trace, "waveMesh").attachments.size() == 1);
    REQUIRE_FALSE(findTraceNode(trace, "multiply").previewable);
    REQUIRE(findTraceNode(trace, "multiply").signalInputs.size() == 2);
    REQUIRE(findTraceNode(trace, "multiply").attachments.empty());
}

TEST_CASE("Runtime keeps scratch attachments separate from signal inputs", "[cycle-v2][runtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto trace = GraphRuntime().process(graph, compileResult.plan);
    const auto& wave = findTraceNode(trace, "waveMesh");

    REQUIRE(wave.attachments.size() == 1);
    REQUIRE(wave.attachments.front().sourceNodeId == "scratchEnv");
    REQUIRE(wave.attachments.front().destPortId == "scratch");
    REQUIRE(wave.attachments.front().domain == PortDomain::EnvelopeSignal);
    REQUIRE(std::none_of(
            wave.signalInputs.begin(),
            wave.signalInputs.end(),
            [](const RuntimeInput& input) {
                return input.destPortId == "scratch";
            }));
}
