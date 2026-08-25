#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Envelope/EnvelopePurpose.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshMeshState.h"

#include <Audio/CycleDsp/IrModel.h>
#include <Curve/Mesh/Mesh.h>

using namespace CycleV2;

TEST_CASE("Envelope purpose changes output grammar and removes stale edges atomically",
        "[cycle-v2][graph][envelope][purpose]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));

    REQUIRE(editor.setNodeParameter(graph, "env", "purpose", "Purpose", "volume").succeeded());
    REQUIRE(editor.connect(
            graph,
            { "env", "env", false },
            { "multiply", "right", true }).succeeded());
    REQUIRE(graph.getEdges().size() == 1);

    const auto changed = editor.setNodeParametersAtomic(graph, "env", {
            { "purpose", "Purpose", "scratch" },
            { "logarithmic", "Logarithmic", "1" }
    });
    REQUIRE(changed.succeeded());
    REQUIRE(changed.changes.removedEdges.size() == 1);
    REQUIRE(graph.getEdges().empty());
    const Node* envelope = graph.findNode("env");
    REQUIRE(envelope != nullptr);
    REQUIRE(envelopePurposeFor(*envelope) == EnvelopePurpose::Scratch);
    REQUIRE(envelope->subtitle == "scratch envelope");
    REQUIRE(envelope->outputs.front().domain == PortDomain::EnvelopeSignal);
    REQUIRE(parameterValueForNode(*envelope, "logarithmic") == "0");

    const auto attached = editor.connect(
            graph,
            { "env", "env", false },
            { "mesh", "scratch", true });
    REQUIRE(attached.succeeded());
    REQUIRE(graph.getEdges().front().connectionKind == ConnectionKind::ProcessingAttachment);
    REQUIRE(graph.getEdges().front().attachmentType == AttachmentType::ScratchEnvelope);
}

TEST_CASE("Envelope purpose exposes control volume and pitch domains",
        "[cycle-v2][graph][envelope][purpose]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));

    const struct Expected {
        const char* purpose;
        PortDomain domain;
        bool logarithmic;
    } expected[] {
            { "control", PortDomain::ControlSignal, true },
            { "volume", PortDomain::EnvelopeSignal, true },
            { "pitch", PortDomain::PitchSignal, false },
            { "scratch", PortDomain::EnvelopeSignal, false }
    };

    for (const auto& item : expected) {
        REQUIRE(editor.setNodeParameter(
                graph,
                "env",
                "purpose",
                "Purpose",
                item.purpose).succeeded());
        const Node* envelope = graph.findNode("env");
        REQUIRE(envelope != nullptr);
        REQUIRE(envelope->outputs.front().domain == item.domain);
        REQUIRE(envelopePurposeAllowsLogarithmic(envelopePurposeFor(*envelope)) == item.logarithmic);
    }
}

TEST_CASE("Pitch Envelope routes only to the typed Voice Context pitch port",
        "[cycle-v2][graph][envelope][purpose][pitch]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    REQUIRE(editor.setNodeParameter(
            graph, "env", "purpose", "Purpose", "pitch").succeeded());

    const auto voicePitch = editor.connect(
            graph,
            { "env", "env", false },
            { "voice", "pitch", true });
    REQUIRE(voicePitch.succeeded());
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(graph.getEdges().front().domain == PortDomain::PitchSignal);
    const auto genericDestination = editor.connect(
            graph,
            { "env", "env", false },
            { "multiply", "right", true });
    REQUIRE(genericDestination.code == GraphEditCode::ValidationRejected);
    REQUIRE(graph.getEdges().size() == 1);
}

TEST_CASE("Envelope purpose edit restores its removed routing through document undo",
        "[cycle-v2][graph][envelope][purpose][undo]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    REQUIRE(editor.setNodeParameter(graph, "env", "purpose", "Purpose", "volume").succeeded());
    REQUIRE(editor.connect(
            graph,
            { "env", "env", false },
            { "multiply", "right", true }).succeeded());

    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const auto changed = commands.setNodeParameter(
            "env", "purpose", "Purpose", "scratch");
    REQUIRE(changed.succeeded());
    REQUIRE(changed.changes.removedEdges.size() == 1);
    REQUIRE(document.graph().getEdges().empty());

    REQUIRE(document.undo());
    const Node* envelope = document.graph().findNode("env");
    REQUIRE(envelope != nullptr);
    REQUIRE(envelopePurposeFor(*envelope) == EnvelopePurpose::Volume);
    REQUIRE(document.graph().getEdges().size() == 1);
    REQUIRE(document.graph().getEdges().front().connectionKind == ConnectionKind::Signal);
}

TEST_CASE("Graph editor rejects normalized no-op parameter attempts", "[cycle-v2][graph][causal]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Delay, "delay", {}));
    const uint64_t graphRevision = graph.getRevision();

    const auto result = GraphEditor().setNodeParameter(
            graph, "delay", "time", "Time", "0.500000");

    REQUIRE(result.succeeded());
    REQUIRE_FALSE(result.changed);
    REQUIRE(graph.getRevision() == graphRevision);
}

TEST_CASE("IR length uses one effective normalizer for graph and DSP edits",
        "[cycle-v2][graph][causal]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));

    const auto first = GraphEditor().setNodeParameter(graph, "ir", "size", "Size", "0.51");
    const auto sameLength = GraphEditor().setNodeParameter(graph, "ir", "size", "Size", "0.56");
    const auto nextLength = GraphEditor().setNodeParameter(graph, "ir", "size", "Size", "0.58");

    REQUIRE(first.succeeded());
    REQUIRE_FALSE(first.changed);
    REQUIRE(sameLength.succeeded());
    REQUIRE_FALSE(sameLength.changed);
    REQUIRE(nextLength.succeeded());
    REQUIRE(nextLength.changed);

    const auto* size = NodeDefinitionRegistry::instance().findParameter(
            NodeKind::ImpulseResponse,
            "size");
    REQUIRE(size != nullptr);
    for (int exponent = 7; exponent <= 14; ++exponent) {
        const int length = 1 << exponent;
        String normalized(CycleDsp::irImpulseLengthValue(length));

        for (int publication = 0; publication < 4; ++publication) {
            normalized = size->normalized(normalized);
            CAPTURE(exponent, length, publication, normalized);
            REQUIRE(CycleDsp::irImpulseLength(normalized.getDoubleValue()) == length);
        }
    }
}

TEST_CASE("Compound editor movement publishes one durable document revision",
        "[cycle-v2][graph][causal]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Delay, "delay", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const uint64_t initialRevision = document.revision();
    int publications {};
    commands.beginCompoundEdit();
    document.setListener([&](uint64_t, const GraphChangeSet&) {
        ++publications;
    });

    REQUIRE(commands.setNodeParameter("delay", "time", "Time", "0.6").succeeded());
    REQUIRE(commands.setNodeParameter("delay", "time", "Time", "0.7").succeeded());
    REQUIRE(commands.setNodeParameter("delay", "time", "Time", "0.8").succeeded());
    REQUIRE(document.revision() == initialRevision);
    REQUIRE(publications == 0);

    commands.commitCompoundEdit();

    REQUIRE(document.revision() == initialRevision + 1);
    REQUIRE(publications == 1);
}

TEST_CASE("Performance keyboard movement is one undoable layout edit",
        "[cycle-v2][graph][layout][keyboard]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    const uint64_t initialRevision = document.revision();

    commands.beginCompoundEdit();
    REQUIRE(commands.setPerformanceKeyboardBounds({ 100.f, 200.f, 496.f, 184.f }).succeeded());
    REQUIRE(commands.setPerformanceKeyboardBounds({ 180.f, 260.f, 496.f, 184.f }).succeeded());
    REQUIRE(document.revision() == initialRevision);
    commands.commitCompoundEdit();

    REQUIRE(document.revision() == initialRevision + 1);
    REQUIRE(document.isDirty());
    REQUIRE(document.graph().getPerformanceKeyboardBounds()
            == Rectangle<float>(180.f, 260.f, 496.f, 184.f));
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.graph().getPerformanceKeyboardBounds().has_value());
}

TEST_CASE("Transient editor movement leaves the durable graph unchanged until commit",
        "[cycle-v2][graph][causal]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Delay, "delay", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const auto timeValue = [](const NodeGraph& source) {
        const Node* node = source.findNode("delay");
        const auto found = std::find_if(
                node->parameters.begin(), node->parameters.end(), [](const auto& parameter) {
                    return parameter.id == "time";
                });
        return found != node->parameters.end() ? found->value : String();
    };
    const String durableValue = timeValue(document.graph());
    const uint64_t initialRevision = document.revision();

    commands.beginTransientEdit();
    REQUIRE(commands.setNodeParameter("delay", "time", "Time", "0.8").changed);
    const String transientValue = timeValue(commands.editingGraph());
    CHECK(timeValue(document.graph()) == durableValue);
    CHECK(transientValue != durableValue);
    CHECK(document.revision() == initialRevision);

    commands.commitTransientEdit();
    CHECK(timeValue(document.graph()) == transientValue);
    CHECK(document.revision() == initialRevision + 1);
}

TEST_CASE("Graph editor connects compatible ports", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "right");

    const auto result = GraphEditor().connect(
            graph,
            { "env", "env", false },
            { "multiply", "right", true });

    REQUIRE(result.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    const auto& edge = graph.getEdges().back();
    REQUIRE(edge.sourceNodeId == "env");
    REQUIRE(edge.destNodeId == "multiply");
    REQUIRE(edge.destPortId == "right");
    REQUIRE_FALSE(edge.isAttachment());
}

TEST_CASE("Graph editor orients input to output connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "right");

    const auto result = GraphEditor().connect(
            graph,
            { "multiply", "right", true },
            { "env", "env", false });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().sourceNodeId == "env");
    REQUIRE(graph.getEdges().back().destNodeId == "multiply");
}

TEST_CASE("Graph editor marks scratch connections as attachments", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("waveMesh", "scratch");

    const auto result = GraphEditor().connect(
            graph,
            { "scratchEnv", "env", false },
            { "waveMesh", "scratch", true });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().isProcessingAttachment());
    REQUIRE(graph.getEdges().back().destPortId == "scratch");
}

TEST_CASE("Graph editor creates targeted Trimesh Guide assignments", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().createGuideCurveAndAssignToTrimeshVertexParameter(
            graph,
            "waveMesh",
            2,
            "amp");

    REQUIRE(result.succeeded());
    REQUIRE(result.nodeId == "guide1");
    REQUIRE(GraphValidator().isValid(graph));

    REQUIRE(graph.getGuideAssignments().size() == 1);
    const auto& assignment = graph.getGuideAssignments().front();
    REQUIRE(assignment.guideId == "guide1");
    REQUIRE(assignment.targetNodeId == "waveMesh");
    REQUIRE(assignment.target.cubeIndex == 0);
    REQUIRE(assignment.target.field == GuideCurveField::Amplitude);
}

TEST_CASE("New Guide resources start flat with neutral modulation",
        "[cycle-v2][graph][guides]") {
    NodeGraph graph;
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());

    const GuideCurveResource* guide = graph.findGuideCurve("guide1");
    REQUIRE(guide != nullptr);
    REQUIRE(guide->noise == 0.f);
    REQUIRE(guide->dcOffset == 0.f);
    REQUIRE(guide->phase == 0.f);

    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(guide->model);
    REQUIRE(model != nullptr);
    REQUIRE(model->flatCurve() != nullptr);
    const auto& vertices = model->flatCurve()->getVertices();
    REQUIRE(vertices.size() == 2);
    REQUIRE(vertices.front().y == 0.5f);
    REQUIRE(vertices.back().y == 0.5f);
}

TEST_CASE("Graph editor shares guide curves across multiple Trimesh targets", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());

    const auto waveResult = GraphEditor().assignGuideCurveToTrimeshVertexParameter(
            graph,
            "guide1",
            "waveMesh",
            1,
            "phase");
    const auto magResult = GraphEditor().assignGuideCurveToTrimeshVertexParameter(
            graph,
            "guide1",
            "magMesh",
            3,
            "amp");

    REQUIRE(waveResult.succeeded());
    REQUIRE(magResult.succeeded());
    REQUIRE(graph.assignGuideCurve({
            "guide1", "waveMesh", { 2, GuideCurveField::Amplitude }
    }));
    REQUIRE(GraphValidator().isValid(graph));

    REQUIRE(graph.getGuideAssignments().size() == 3);
    REQUIRE(graph.guideUsageCount("guide1") == 3);
    REQUIRE(graph.guideTargetNodeIds("guide1").size() == 2);
    REQUIRE(graph.guideTargetNodeIds("guide1")[0] == "waveMesh");
    REQUIRE(graph.guideTargetNodeIds("guide1")[1] == "magMesh");
    REQUIRE(graph.guideIdsForTargetNode("waveMesh") == std::vector<String> { "guide1" });
}

TEST_CASE("Guide resource edits replace the resource model without creating a node", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    GraphEditor editor;
    REQUIRE(editor.createGuideCurve(graph).succeeded());

    const GuideCurveResource* original = graph.findGuideCurve("guide1");
    REQUIRE(original != nullptr);
    REQUIRE(editor.replaceGuideCurve(graph, "guide1", original->model, {
            { "enabled", "Enabled", "0" },
            { "noise", "Noise", "0.2" },
            { "dcOffset", "DC Offset", "0.7" },
            { "phase", "Phase", "0.9" }
    }).succeeded());

    const GuideCurveResource* edited = graph.findGuideCurve("guide1");
    REQUIRE(edited != nullptr);
    REQUIRE_FALSE(edited->enabled);
    REQUIRE(edited->noise == 0.2f);
    REQUIRE(edited->dcOffset == 0.7f);
    REQUIRE(edited->phase == 0.9f);
    REQUIRE(graph.findNode("guide1") == nullptr);
}

TEST_CASE("Guide resource gestures publish two transient updates and undo once",
        "[cycle-v2][graph][guides][gesture]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    REQUIRE(commands.assignGuideCurve("guide1", "waveMesh", 2, "amp").succeeded());
    const GuideCurveResource* original = document.graph().findGuideCurve("guide1");
    REQUIRE(original != nullptr);
    const NodeModelStatePtr originalModel = original->model;
    const uint64_t durableRevision = originalModel->revision();

    const auto publication = [&](float noise) {
        return GuideCurveStatePublication {
                "guide1",
                durableRevision,
                originalModel,
                {
                    { "enabled", "Enabled", "1" },
                    { "noise", "Noise", String(noise) },
                    { "dcOffset", "DC Offset", "0.5" },
                    { "phase", "Phase", "0.5" }
                }
        };
    };

    commands.beginTransientEdit();
    REQUIRE(commands.publishGuideCurveState(publication(0.2f)).succeeded());
    REQUIRE(document.graph().findGuideCurve("guide1")->noise == 0.f);
    REQUIRE(commands.editingGraph().findGuideCurve("guide1")->noise == 0.2f);
    REQUIRE(commands.publishGuideCurveState(publication(0.8f)).succeeded());
    REQUIRE(commands.editingGraph().findGuideCurve("guide1")->noise == 0.8f);
    REQUIRE(commands.transientChanges().guidesChanged);
    REQUIRE(commands.transientChanges().nodeIds == std::vector<String> { "waveMesh" });
    commands.commitTransientEdit();

    REQUIRE(document.graph().findGuideCurve("guide1")->noise == 0.8f);
    REQUIRE(commands.publishGuideCurveState(publication(0.4f)).code
            == GraphEditCode::ConflictingRevision);
    GuideCurveStatePublication incomplete = publication(0.4f);
    incomplete.controls.pop_back();
    REQUIRE(commands.publishGuideCurveState(incomplete).code
            == GraphEditCode::InvalidControlValue);
    REQUIRE(document.undo());
    REQUIRE(document.graph().findGuideCurve("guide1")->noise == 0.f);
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
}

TEST_CASE("Guide resource names are document content and undoable", "[cycle-v2][graph]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    REQUIRE(commands.renameGuideCurve("guide1", "Vibrato Bend").succeeded());
    REQUIRE(document.graph().findGuideCurve("guide1")->name == "Vibrato Bend");
    REQUIRE(document.undo());
    REQUIRE(document.graph().findGuideCurve("guide1")->name.isEmpty());
}

TEST_CASE("Guide resource duplication copies content without copying assignments", "[cycle-v2][graph]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    REQUIRE(commands.renameGuideCurve("guide1", "Vibrato Bend").succeeded());
    REQUIRE(commands.assignGuideCurve("guide1", "waveMesh", 2, "amp").succeeded());

    const auto duplicated = commands.duplicateGuideCurve("guide1");
    REQUIRE(duplicated.succeeded());
    REQUIRE(duplicated.nodeId == "guide2");
    const GuideCurveResource* copy = document.graph().findGuideCurve("guide2");
    REQUIRE(copy != nullptr);
    REQUIRE(copy->shortLabel == "G2");
    REQUIRE(copy->name == "Vibrato Bend Copy");
    REQUIRE(copy->model == document.graph().findGuideCurve("guide1")->model);
    REQUIRE(document.graph().getGuideAssignments().size() == 1);

    REQUIRE(document.undo());
    REQUIRE(document.graph().findGuideCurve("guide2") == nullptr);
}

TEST_CASE("Guide resource reordering preserves its identity and assignments", "[cycle-v2][graph]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.createGuideCurve().succeeded());
    REQUIRE(commands.createGuideCurve().succeeded());
    REQUIRE(commands.assignGuideCurve("guide1", "waveMesh", 2, "amp").succeeded());

    REQUIRE(commands.reorderGuideCurve("guide2", 0).succeeded());
    const auto& guides = document.graph().getGuideCurves();
    REQUIRE(guides[0].id == "guide2");
    REQUIRE(guides[0].shortLabel == "G2");
    REQUIRE(guides[0].shelfOrder == 0);
    REQUIRE(guides[1].id == "guide1");
    REQUIRE(guides[1].shelfOrder == 1);
    REQUIRE(document.graph().getGuideAssignments().front().guideId == "guide1");

    REQUIRE(document.undo());
    REQUIRE(document.graph().getGuideCurves()[0].id == "guide1");
}

TEST_CASE("Graph editor replaces existing Trimesh guide attachment target", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(GraphEditor().assignGuideCurveToTrimeshVertexParameter(
            graph,
            "guide1",
            "waveMesh",
            2,
            "amp").succeeded());

    const auto result = GraphEditor().assignGuideCurveToTrimeshVertexParameter(
            graph,
            "guide2",
            "waveMesh",
            2,
            "amp");

    REQUIRE(result.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    REQUIRE(graph.getGuideAssignments().size() == 1);
    REQUIRE(graph.getGuideAssignments().front().guideId == "guide2");
    REQUIRE(graph.guideUsageCount("guide1") == 0);
    REQUIRE(graph.guideUsageCount("guide2") == 1);
    REQUIRE(graph.guideIdsForTargetNode("waveMesh") == std::vector<String> { "guide2" });
}

TEST_CASE("Trimesh topology edits reconcile Guide assignments in one undoable command",
        "[cycle-v2][graph][guides]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(graph.findNode("waveMesh") != nullptr);
    REQUIRE(graph.assignGuideCurve({
            "guide1", "waveMesh", { 0, GuideCurveField::Amplitude }
    }));
    REQUIRE(graph.assignGuideCurve({
            "guide1", "waveMesh", { 1, GuideCurveField::Amplitude }
    }));
    REQUIRE(GraphValidator().isValid(graph));

    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    const Node* original = document.graph().findNode("waveMesh");
    REQUIRE(original != nullptr);
    const uint64_t revision = original->model->revision();
    Mesh replacement("SingleCubeTrimesh");
    TrimeshMeshFactory::addVoiceCube(replacement, 0.f, 1.f, 0.2f, 0.8f, 0.5f);
    const NodeModelStatePtr replacementModel = TrimeshNodeModelState::copyOf(
            replacement,
            revision + 1);
    replacement.destroy();

    const auto result = commands.replaceNodeModel(
            "waveMesh",
            revision,
            replacementModel);

    REQUIRE(result.succeeded());
    REQUIRE(result.changes.guidesChanged);
    REQUIRE(result.changes.guidePresentationChanged);
    REQUIRE(document.graph().getGuideAssignments().size() == 1);
    REQUIRE(document.graph().getGuideAssignments().front().target.cubeIndex == 0);
    REQUIRE(document.graph().guideUsageCount("guide1") == 1);
    REQUIRE(GraphValidator().isValid(document.graph()));

    REQUIRE(document.undo());
    REQUIRE(document.graph().getGuideAssignments().size() == 2);
    REQUIRE(document.graph().guideUsageCount("guide1") == 2);
    REQUIRE(GraphValidator().isValid(document.graph()));
}

TEST_CASE("Graph editor colours universal output edges from typed destinations", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Multiply, "mul", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "out", { 260.f, 0.f }));

    const auto result = GraphEditor().connect(
            graph,
            { "mul", "out", false },
            { "out", "time", true });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().domain == PortDomain::TimeSignal);
}

TEST_CASE("Graph editor splices a node into an edge", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 520.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto result = GraphEditor().spliceNodeIntoEdge(graph, 0, "shape");

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().size() == 2);
    REQUIRE(graph.getEdges()[0].sourceNodeId == "wave");
    REQUIRE(graph.getEdges()[0].destNodeId == "shape");
    REQUIRE(graph.getEdges()[1].sourceNodeId == "shape");
    REQUIRE(graph.getEdges()[1].destNodeId == "out");
    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Graph editor rejects incompatible edge splices", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 520.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto result = GraphEditor().spliceNodeIntoEdge(graph, 0, "fft");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(graph.getEdges()[0].sourceNodeId == "wave");
    REQUIRE(graph.getEdges()[0].destNodeId == "out");
}

TEST_CASE("Graph editor rejects multiply splices on phase edges", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 520.f, 0.f }));
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });

    const auto result = GraphEditor().spliceNodeIntoEdge(graph, 0, "multiply");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(graph.getEdges()[0].sourceNodeId == "fft");
    REQUIRE(graph.getEdges()[0].destNodeId == "ifft");
}

TEST_CASE("Graph editor rejects incompatible connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "out", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().connect(
            graph,
            { "pitch", "out", false },
            { "multiply", "left", true });

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(graph.getEdges().size() == edgeCount);
}

TEST_CASE("Graph editor rejects context outputs on ordinary signal inputs", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 260.f, 0.f }));
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "multiply", "left", true });

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE(graph.getEdges().size() == edgeCount);
}

TEST_CASE("Graph editor removes nodes and incident edges", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeNode(graph, "fft");

    REQUIRE(result.succeeded());

    for (const auto& node : graph.getNodes()) {
        REQUIRE(node.id != "fft");
    }

    for (const auto& edge : graph.getEdges()) {
        REQUIRE(edge.sourceNodeId != "fft");
        REQUIRE(edge.destNodeId != "fft");
    }
}

TEST_CASE("Graph editor reports missing node removal", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeNode(graph, "missing");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingNode);
}

TEST_CASE("Graph editor removes edges by index", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().removeEdgeAt(graph, 0);

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().size() == edgeCount - 1);
}

TEST_CASE("Graph editor reports missing edge removal", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeEdgeAt(graph, graph.getEdges().size());

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingEdge);
}

TEST_CASE("Graph editor updates node parameters", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    GraphEditor editor;
    const size_t initialParameterCount = graph.getNodes().front().parameters.size();

    const auto updateResult = editor.setNodeParameter(
            graph,
            "voice",
            "domain",
            "Start Domain",
            "spectral");

    REQUIRE(updateResult.succeeded());
    REQUIRE(updateResult.nodeId == "voice");
    REQUIRE(parameterValueForNode(graph.getNodes().front(), "domain") == "spectral");

    const auto addResult = editor.setNodeParameter(
            graph,
            "voice",
            "tempoSync",
            "Tempo Sync",
            "true");

    REQUIRE_FALSE(addResult.succeeded());
    REQUIRE(addResult.code == GraphEditCode::UnknownParameter);
    REQUIRE(parameterValueForNode(graph.getNodes().front(), "tempoSync").isEmpty());
    REQUIRE(graph.getNodes().front().parameters.size() == initialParameterCount);
}

TEST_CASE("Graph editor validates and normalizes declared parameters", "[cycle-v2][graph][definitions]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));

    const auto normalized = GraphEditor().setNodeParameter(
            graph, "voice", "portamento", "Ignored Label", "true");
    const auto invalid = GraphEditor().setNodeParameter(
            graph, "voice", "octave", "Octave", "99");

    REQUIRE(normalized.succeeded());
    REQUIRE(parameterValueForNode(*graph.findNode("voice"), "portamento") == "1");
    REQUIRE(normalized.changes.parameterImpacts != ParameterImpact::None);
    REQUIRE_FALSE(invalid.succeeded());
    REQUIRE(invalid.code == GraphEditCode::InvalidParameterValue);
}

TEST_CASE("Graph editor reports missing node parameter updates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().setNodeParameter(
            graph,
            "missing",
            "domain",
            "Start Domain",
            "spectral");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingNode);
}
