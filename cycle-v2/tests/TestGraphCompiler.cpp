#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"

#include <algorithm>

using namespace CycleV2;

namespace {

Port input(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, true };
}

Port output(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, false };
}

Node graphNode(String id, std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        id,
        NodeKind::GenericProcessor,
        id,
        {},
        {},
        {},
        std::move(inputs),
        std::move(outputs)
    };
}

int orderIndex(const GraphExecutionPlan& plan, const String& nodeId) {
    const auto found = std::find(plan.nodeOrder.begin(), plan.nodeOrder.end(), nodeId);
    REQUIRE(found != plan.nodeOrder.end());
    return static_cast<int>(std::distance(plan.nodeOrder.begin(), found));
}

const Edge& findSignalEdge(const GraphExecutionPlan& plan, const String& sourceNodeId, const String& destNodeId) {
    const auto found = std::find_if(
            plan.signalEdges.begin(),
            plan.signalEdges.end(),
            [&](const Edge& edge) {
                return edge.sourceNodeId == sourceNodeId && edge.destNodeId == destNodeId;
            });

    REQUIRE(found != plan.signalEdges.end());
    return *found;
}

}

TEST_CASE("Demo graph compiles to a stable execution order", "[cycle-v2][graph]") {
    const auto result = GraphCompiler().compile(NodeGraph::createDemoGraph());

    REQUIRE(result.succeeded());
    REQUIRE(result.plan.attachments.size() == 2);
    REQUIRE(result.plan.signalEdges.size() == 11);
    REQUIRE(result.plan.steps.size() == result.plan.nodeOrder.size());

    const auto& plan = result.plan;
    REQUIRE(orderIndex(plan, "voice") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "waveMesh") < orderIndex(plan, "fft"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "magMesh"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "magMesh") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "phaseMesh") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "addMag") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "addPhase") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "ifft") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "env") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "multiply") < orderIndex(plan, "out"));

    const auto findStep = [&](const String& nodeId) -> const GraphExecutionStep& {
        const auto found = std::find_if(
                plan.steps.begin(),
                plan.steps.end(),
                [&](const GraphExecutionStep& step) {
                    return step.nodeId == nodeId;
                });
        REQUIRE(found != plan.steps.end());
        return *found;
    };

    REQUIRE(parameterValueForNode({ "voice", NodeKind::VoiceContext, {}, {}, {}, findStep("voice").parameters, {}, {} },
            "domain") == "waveform");
    REQUIRE(findStep("waveMesh").audioRole == AudioModuleRole::MeshSource);
    REQUIRE(findStep("waveMesh").previewRole == PreviewModuleRole::MeshSurface);
    REQUIRE(findStep("waveMesh").cycle1AdapterBacked);
    REQUIRE(findStep("waveMesh").cycle1Reference
            == "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp");
    REQUIRE(findStep("fft").audioRole == AudioModuleRole::Fft);
    REQUIRE_FALSE(findStep("fft").previewable);
    REQUIRE(findStep("multiply").audioRole == AudioModuleRole::Multiply);
    REQUIRE(findStep("out").previewRole == PreviewModuleRole::OutputMeters);
}

TEST_CASE("Invalid graphs do not compile", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "voice", "context", "multiply", "audio", PortDomain::DomainContext, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.empty());
    REQUIRE(result.plan.nodeOrder.empty());
}

TEST_CASE("Compiler resolves source domains from voice context parameters", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "next", { 760.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });
    graph.addEdge({ "add", "out", "next", "left", PortDomain::ControlSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findSignalEdge(result.plan, "mesh", "add").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findSignalEdge(result.plan, "add", "next").domain == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Compiler rejects processing cycles", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(graphNode(
            "a",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addNode(graphNode(
            "b",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addEdge({ "a", "out", "b", "in", PortDomain::TimeSignal, false });
    graph.addEdge({ "b", "out", "a", "in", PortDomain::TimeSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.size() == 1);
    REQUIRE(result.compileIssues.front().code == GraphCompileCode::CycleDetected);
    REQUIRE(result.plan.nodeOrder.empty());
}
