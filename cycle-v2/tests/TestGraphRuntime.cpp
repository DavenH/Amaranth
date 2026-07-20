#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Runtime/GraphPresentationModel.h"
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
    REQUIRE(findTraceNode(trace, "voice").parameters.size() == 6);
    REQUIRE(findTraceNode(trace, "voice").signalInputs.empty());
    REQUIRE(findTraceNode(trace, "fft").cycleFrames == 2048);
    REQUIRE(findTraceNode(trace, "fft").latencyCycles == 0);
    REQUIRE(findTraceNode(trace, "fft").transformMode == "cycle");
    REQUIRE(findTraceNode(trace, "waveMesh").kind == NodeKind::TrilinearMesh);
    REQUIRE(findTraceNode(trace, "waveMesh").audioRole == AudioModuleRole::MeshSource);
    REQUIRE(findTraceNode(trace, "waveMesh").previewRole == PreviewModuleRole::MeshSurface);
    REQUIRE(findTraceNode(trace, "waveMesh").cycle1AdapterBacked);
    REQUIRE(findTraceNode(trace, "waveMesh").cycle1Reference
            == "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp");
    REQUIRE(findTraceNode(trace, "waveMesh").signalInputs.size() == 1);
    REQUIRE(findTraceNode(trace, "waveMesh").attachments.size() == 1);
    REQUIRE_FALSE(findTraceNode(trace, "multiply").previewable);
    REQUIRE(findTraceNode(trace, "multiply").signalInputs.size() == 2);
    REQUIRE(findTraceNode(trace, "multiply").attachments.empty());
    REQUIRE(findTraceNode(trace, "fft").signalOutputs.size() == 2);
    REQUIRE(findTraceNode(trace, "fft").signalOutputs[0].portId == "mag");
    REQUIRE(findTraceNode(trace, "fft").signalOutputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findTraceNode(trace, "fft").signalOutputs[1].portId == "phase");
    REQUIRE(findTraceNode(trace, "fft").signalOutputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findTraceNode(trace, "magMesh").signalOutputs.front().domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findTraceNode(trace, "phaseMesh").signalOutputs.front().domain == PortDomain::SpectralPhaseSignal);
}

TEST_CASE("Queued presentation publication is inert after model destruction",
        "[cycle-v2][runtime][causal]") {
    ScopedJuceInitialiser_GUI juce;
    NodeGraph graph = NodeGraph::createDemoGraph();
    bool completed {};
    {
        GraphPresentationModel presentation;
        GraphChangeSet topology;
        topology.topologyChanged = true;
        REQUIRE(presentation.refresh(graph, 1, topology));

        const auto mesh = std::find_if(
                graph.getNodes().begin(), graph.getNodes().end(), [](const auto& node) {
                    return node.kind == NodeKind::TrilinearMesh;
                });
        REQUIRE(mesh != graph.getNodes().end());
        const auto edit = GraphEditor().setNodeParameter(
                graph, mesh->id, "red", "Red", "0.7");
        REQUIRE(edit.succeeded());
        REQUIRE(edit.changed);
        presentation.refreshAsync(graph, 2, edit.changes, [&] {
            completed = true;
        });
    }

    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE_FALSE(completed);
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

TEST_CASE("Runtime exposes targeted guide attachments separately from signal inputs", "[cycle-v2][runtime]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createAndAttachGuideCurveToTrimeshVertexParameter(
            graph,
            "waveMesh",
            4,
            "curve",
            { 100.f, 100.f }).succeeded());

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto trace = GraphRuntime().process(graph, compileResult.plan);
    const auto& wave = findTraceNode(trace, "waveMesh");

    REQUIRE(std::any_of(
            wave.attachments.begin(),
            wave.attachments.end(),
            [](const RuntimeInput& input) {
                return input.sourceNodeId == "guide"
                    && input.destPortId == "guide.vertex.4.curve";
            }));
    REQUIRE(std::none_of(
            wave.signalInputs.begin(),
            wave.signalInputs.end(),
            [](const RuntimeInput& input) {
                return input.destPortId.startsWith("guide.vertex.");
            }));
}

TEST_CASE("Ordinary DSP edits refresh configuration without compiling topology",
        "[cycle-v2][runtime][causal]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Delay, "delay", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(document.graph(), document.revision()));
    const size_t initialCompilations = presentation.compilationCount();

    const auto result = commands.setNodeParameter("delay", "time", "Time", "0.75");
    REQUIRE(result.succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));

    REQUIRE(presentation.compilationCount() == initialCompilations);
    REQUIRE(presentation.previewRenderCount() == 2);
}
