#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <utility>
#include <vector>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/FlatCurvePanelAdapter.h"
#include "Nodes/Envelope/Editor/EnvelopePanelAdapter.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

namespace {

NodeGraph scaledGraph(int unrelatedNodeCount, size_t audioSampleCount) {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    for (int index = 0; index < unrelatedNodeCount; ++index) {
        graph.addNode(factory.createNode(
                NodeKind::Add,
                "unrelated" + String(index),
                { (float) index, (float) index }));
    }

    AudioSampleResource audio { "audio", "Unrelated.wav", 48000.0, {} };
    audio.samples.resize(audioSampleCount);
    graph.addAudioResource(std::move(audio));
    return graph;
}

}

TEST_CASE("Scalar gesture cost is independent of unrelated graph and audio data",
        "[cycle-v2][complexity][gesture][parameter]") {
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        GraphDocument document(scaledGraph(scale.first, scale.second));
        GraphCommandDispatcher commands(document);
        int causalPublications {};
        GraphChangeSet causalChange;
        document.setListener([&](uint64_t, const GraphChangeSet& change) {
            ++causalPublications;
            causalChange = change;
        });
        InteractionComplexityDiagnostics::reset();

        commands.beginTransientEdit();
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.6").succeeded());
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.7").succeeded());
        const auto noChange = commands.setNodeParameter("output", "gain", "Gain", "0.7");
        REQUIRE(noChange.succeeded());
        REQUIRE_FALSE(noChange.changed);
        REQUIRE(document.graph().findNodeParameter("output", "gain")->value != "0.7");
        REQUIRE(commands.editingGraph().findNodeParameter("output", "gain")->value == "0.7");
        const auto& editingNodes = commands.editingGraph().getNodes();
        const auto output = std::find_if(editingNodes.begin(), editingNodes.end(), [](const auto& node) {
            return node.id == "output";
        });
        REQUIRE(output != editingNodes.end());
        REQUIRE(parameterValueForNode(*output, "gain") == "0.7");
        commands.commitTransientEdit();
        REQUIRE(causalPublications == 1);
        REQUIRE(causalChange.nodeIds == std::vector<String> { "output" });
        REQUIRE(causalChange.parameterImpacts != ParameterImpact::None);

        REQUIRE(document.undo());
        REQUIRE(document.redo());
        REQUIRE(causalPublications == 3);
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("Canvas translation iterates changed identities without graph snapshots",
        "[cycle-v2][complexity][gesture][canvas]") {
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        GraphDocument document(scaledGraph(scale.first, scale.second));
        GraphCommandDispatcher commands(document);
        const Point<float> original = document.graph().findNode("output")->bounds.getPosition();
        InteractionComplexityDiagnostics::reset();

        commands.beginCompoundEdit();
        REQUIRE(commands.translateNodes({ "output" }, { 4.f, 3.f }).succeeded());
        REQUIRE(commands.translateNodes({ "output" }, { 2.f, 1.f }).succeeded());
        commands.commitCompoundEdit();
        REQUIRE(document.graph().findNode("output")->bounds.getPosition()
                == original.translated(6.f, 4.f));

        REQUIRE(document.undo());
        REQUIRE(document.graph().findNode("output")->bounds.getPosition() == original);
        REQUIRE(document.redo());
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.nodeLinearScans == 0);
    }
}

TEST_CASE("Guide curve gesture resolves consumers from the relationship index",
        "[cycle-v2][complexity][gesture][guide]") {
    for (const int unrelatedNodeCount : { 0, 128 }) {
        CAPTURE(unrelatedNodeCount);
        NodeGraph graph = scaledGraph(unrelatedNodeCount, unrelatedNodeCount == 0 ? 0 : 16384);
        const NodeModelStatePtr model = createDefaultGuideCurveModel();
        REQUIRE(graph.addGuideCurve({
                "guide", "G", "Guide", 0, 0, true,
                0.f, 0.f, 0.f, -1, model, {}, 1
        }));
        if (unrelatedNodeCount == 0) {
            REQUIRE(graph.assignGuideCurve({
                    "guide", "output", { 0, GuideCurveField::Amplitude }
            }));
        } else {
            for (int index = 0; index < unrelatedNodeCount; ++index) {
                REQUIRE(graph.assignGuideCurve({
                        "guide",
                        "unrelated" + String(index),
                        { index, GuideCurveField::Amplitude }
                }));
            }
        }

        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        const std::vector<NodeParameter> firstControls {
                { "enabled", "Enabled", "1" },
                { "noise", "Noise", "0.25" },
                { "dcOffset", "DC Offset", "0" },
                { "phase", "Phase", "0" }
        };
        auto secondControls = firstControls;
        secondControls[1].value = "0.5";
        InteractionComplexityDiagnostics::reset();

        commands.beginTransientEdit();
        REQUIRE(commands.publishGuideCurveState({
                "guide", 1, model, firstControls
        }).succeeded());
        REQUIRE(commands.publishGuideCurveState({
                "guide", 1, model, secondControls
        }).succeeded());
        const auto noChange = commands.publishGuideCurveState({
                "guide", 1, model, secondControls
        });
        REQUIRE(noChange.succeeded());
        REQUIRE_FALSE(noChange.changed);
        commands.commitTransientEdit();
        REQUIRE(document.undo());
        REQUIRE(document.redo());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.assignmentLinearScans == 0);
    }
}

TEST_CASE("Flat curve movement notifications do not serialize or snapshot the model",
        "[cycle-v2][complexity][gesture][curve]") {
    for (const int extraVertexCount : { 0, 128 }) {
        CAPTURE(extraVertexCount);
        FlatCurvePanelAdapter adapter(NodeKind::Waveshaper);
        adapter.initialiseDefaultMesh();
        for (int index = 0; index < extraVertexCount; ++index) {
            adapter.mesh().addVertex(new Vertex(
                    (float) index / 128.f,
                    (float) index / 256.f));
        }
        InteractionComplexityDiagnostics::reset();

        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.25f;
        REQUIRE(adapter.registerMeshEdit());
        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.5f;
        REQUIRE(adapter.registerMeshEdit());
        REQUIRE(adapter.registerMeshEdit());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.meshCopies == 0);
    }
}

TEST_CASE("Envelope movement notifications do not compare or snapshot the mesh",
        "[cycle-v2][complexity][gesture][envelope]") {
    for (const int extraVertexCount : { 0, 128 }) {
        CAPTURE(extraVertexCount);
        EnvelopePanelAdapter adapter;
        adapter.initialiseDefaultMesh();
        REQUIRE_FALSE(adapter.mesh().getVerts().empty());
        for (int index = 0; index < extraVertexCount; ++index) {
            adapter.mesh().addVertex(new Vertex(
                    (float) index / 128.f,
                    (float) index / 256.f));
        }
        InteractionComplexityDiagnostics::reset();

        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.25f;
        REQUIRE(adapter.registerMeshEdit());
        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.5f;
        REQUIRE(adapter.registerMeshEdit());
        REQUIRE(adapter.registerMeshEdit());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.meshCopies == 0);
    }
}

}
