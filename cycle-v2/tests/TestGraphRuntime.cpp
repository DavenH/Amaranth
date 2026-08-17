#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Waveshaper/WaveshaperSignalProcessor.h"
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

const GraphPreviewResult::SignalProbePreview& findProbePreview(
        const GraphPreviewResult& result,
        const String& probeId) {
    const auto found = std::find_if(
            result.probes.begin(),
            result.probes.end(),
            [&](const auto& preview) {
                return preview.probeId == probeId;
            });

    REQUIRE(found != result.probes.end());
    return *found;
}

const NodePreviewResult& findNodePreview(
        const GraphPreviewResult& result,
        const String& nodeId) {
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [&](const auto& preview) {
                return preview.nodeId == nodeId;
            });

    REQUIRE(found != result.nodes.end());
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
    REQUIRE(findTraceNode(trace, "voice").parameters.size() == 5);
    REQUIRE(std::none_of(
            findTraceNode(trace, "voice").parameters.begin(),
            findTraceNode(trace, "voice").parameters.end(),
            [](const NodeParameter& parameter) {
                return parameter.id == "voices";
            }));
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

TEST_CASE("Runtime prepares targeted Guide assignments without graph attachments", "[cycle-v2][runtime]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createGuideCurveAndAssignToTrimeshVertexParameter(
            graph,
            "waveMesh",
            4,
            "curve").succeeded());

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    REQUIRE(graph.getEdges().size() == NodeGraph::createDemoGraph().getEdges().size());
    REQUIRE(graph.getGuideAssignments().size() == 1);
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

TEST_CASE("Adding a second signal probe refreshes its compiled preview address",
        "[cycle-v2][runtime][probe][causal]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Reverb, "reverb", {}));
    graph.addNode(factory.createNode(NodeKind::Equalizer, "equalizer", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "reverb", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "reverb", "time", "equalizer", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "equalizer", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(document.graph(), document.revision()));
    const size_t initialCompilations = presentation.compilationCount();

    REQUIRE(commands.toggleSignalProbe(1, 0.4f).succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(commands.toggleSignalProbe(2, 0.6f).succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));

    REQUIRE(presentation.compilationCount() == initialCompilations);
    REQUIRE(presentation.previewResult().probes.size() == 2);
    REQUIRE(presentation.previewResult().probes[0].connected);
    REQUIRE(presentation.previewResult().probes[1].connected);
}

TEST_CASE("Stengah probes reflect an asynchronous Waveshaper curve edit at the correct taps",
        "[cycle-v2][runtime][probe][causal][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    ScopedJuceInitialiser_GUI juce;
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("stengah.cyclegraph");
    REQUIRE(preset.existsAsFile());

    GraphDocument document(GraphSerializer().fromJsonString(preset.loadFileAsString()));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(document.graph(), document.revision(), topology));
    REQUIRE(findProbePreview(presentation.previewResult(), "probe2").connected);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe5").connected);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe").connected);
    const auto initialWaveshaperOutput = findProbePreview(
            presentation.previewResult(), "probe2").values;
    const auto initialWaveshaperInput = findProbePreview(
            presentation.previewResult(), "probe5").values;
    const auto initialDownstreamValues = findProbePreview(
            presentation.previewResult(), "probe").values;
    const auto initialMagnitudePreview = findNodePreview(
            presentation.previewResult(), "magnitudeLayer1").primary;

    const Node* waveshaper = document.graph().findNode("waveshaper");
    REQUIRE(waveshaper != nullptr);
    const auto currentModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
            waveshaper->model);
    REQUIRE(currentModel != nullptr);
    REQUIRE(currentModel->flatCurve() != nullptr);
    FlatCurveModel editedCurve;
    REQUIRE(editedCurve.copyFrom(*currentModel->flatCurve()));
    const auto vertices = editedCurve.getVertices();
    std::vector<FlatCurveVertex> movableVertices;
    std::copy_if(
            vertices.begin(),
            vertices.end(),
            std::back_inserter(movableVertices),
            [](const auto& vertex) { return vertex.curve < 0.9f; });
    const auto editedVertex = *std::min_element(
            movableVertices.begin(),
            movableVertices.end(),
            [](const auto& left, const auto& right) {
                return std::abs(left.x - 0.5f) < std::abs(right.x - 0.5f);
            });
    REQUIRE(editedCurve.moveVertex(
            editedVertex.id,
            { editedVertex.x, editedVertex.y > 0.5f ? 0.15f : 0.85f }).succeeded());
    const auto editedModel = CurveNodeModelState::copyOf(
            editedCurve,
            waveshaper->model->revision() + 1);
    const auto beforeConfiguration = WaveshaperSignalProcessor::buildConfiguration(
            waveshaper->parameters, waveshaper->model);
    const auto afterConfiguration = WaveshaperSignalProcessor::buildConfiguration(
            waveshaper->parameters, editedModel);
    REQUIRE(beforeConfiguration != nullptr);
    REQUIRE(afterConfiguration != nullptr);
    REQUIRE(beforeConfiguration->transfer->lookup(editedVertex.x)
            != afterConfiguration->transfer->lookup(editedVertex.x));
    const auto edit = commands.publishCurveState({
            waveshaper->id,
            waveshaper->model->revision(),
            editedModel,
            waveshaper->parameters
    });
    REQUIRE(edit.succeeded());
    REQUIRE(edit.changed);

    bool completed {};
    presentation.refreshAsync(
            document.graph(),
            document.revision(),
            document.lastChange(),
            [&] { completed = true; });
    for (int attempt = 0; attempt < 200 && !completed; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(10);
    }

    REQUIRE(completed);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe2").connected);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe5").connected);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe").connected);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe2").values
            != initialWaveshaperOutput);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe5").values
            == initialWaveshaperInput);
    REQUIRE(findProbePreview(presentation.previewResult(), "probe").values
            != initialDownstreamValues);
    REQUIRE(findNodePreview(presentation.previewResult(), "magnitudeLayer1").primary
            == initialMagnitudePreview);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Stengah probes preserve normalized grids across spectral pan edits",
        "[cycle-v2][runtime][causal][pan][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    ScopedJuceInitialiser_GUI juce;
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("stengah.cyclegraph");
    REQUIRE(preset.existsAsFile());

    NodeGraph graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    graph.addSignalProbe({
            "upstreamMagnitude",
            "magnitudeLayer1",
            "out",
            "magnitudeLayer1Process",
            "in",
            "Upstream magnitude",
            0.5f,
            8
    });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(document.graph(), document.revision(), topology));
    const auto upstreamPrimary = findNodePreview(
            presentation.previewResult(), "magnitudeLayer1").primary;
    const auto upstreamSecondary = findNodePreview(
            presentation.previewResult(), "magnitudeLayer1").secondary;
    const auto upstreamSignal = findProbePreview(
            presentation.previewResult(), "upstreamMagnitude").values;
    const auto downstreamSignal = findProbePreview(
            presentation.previewResult(), "probe3").values;
    REQUIRE(downstreamSignal == upstreamSignal);
    const size_t upstreamProcessCount = presentation.previewAudioProcessCount(
            "magnitudeLayer1");

    for (const String pan : { "1", "0.5", "0", "0.5" }) {
        REQUIRE(commands.setNodeParameter(
                "magnitudeLayer1Process", "pan", "Pan", pan).succeeded());
        REQUIRE(presentation.refresh(
                document.graph(),
                document.revision(),
                document.lastChange()));
        REQUIRE(findNodePreview(
                presentation.previewResult(), "magnitudeLayer1").primary
                == upstreamPrimary);
        REQUIRE(findNodePreview(
                presentation.previewResult(), "magnitudeLayer1").secondary
                == upstreamSecondary);
        REQUIRE(findProbePreview(
                presentation.previewResult(), "upstreamMagnitude").values
                == upstreamSignal);
        REQUIRE(findProbePreview(
                presentation.previewResult(), "probe3").values
                == downstreamSignal);
        REQUIRE(presentation.previewAudioProcessCount("magnitudeLayer1")
                == upstreamProcessCount);
    }

    const Node* magnitude = document.graph().findNode("magnitudeLayer1");
    REQUIRE(magnitude != nullptr);
    const String currentRed = parameterValueForNode(*magnitude, "red");
    const String editedRed = currentRed.getFloatValue() < 0.5f ? "0.8" : "0.2";
    REQUIRE(commands.setNodeParameter(
            "magnitudeLayer1", "red", "Red", editedRed).succeeded());
    REQUIRE(presentation.refresh(
            document.graph(),
            document.revision(),
            document.lastChange()));
    REQUIRE(presentation.previewAudioProcessCount("magnitudeLayer1")
            == upstreamProcessCount + 1);
    REQUIRE(findNodePreview(
            presentation.previewResult(), "magnitudeLayer1").primary
            != upstreamPrimary);
    REQUIRE(findProbePreview(
            presentation.previewResult(), "probe3").values
            != downstreamSignal);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
