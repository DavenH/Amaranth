#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/UI/NodeCanvasScene.h"
#include "../src/UI/NodeCanvasEditorCoordinator.h"
#include "../src/UI/NodeCableRenderer.h"
#include "../src/UI/NodeCanvasViewport.h"
#include "../src/UI/NodePalette.h"
#include "../src/UI/NodeViewModule.h"
#include "../src/UI/SignalProbeRail.h"
#include "../src/UI/TransformCompactEditor.h"
#include "../src/UI/VoiceContextCompactEditor.h"
#include "../src/Runtime/GraphPresentationModel.h"

using namespace CycleV2;

namespace {

uint64_t imageChecksum(const Image& image) {
    const Image::BitmapData pixels(image, Image::BitmapData::readOnly);
    uint64_t checksum = 1469598103934665603ULL;

    for (int y = 0; y < pixels.height; ++y) {
        for (int x = 0; x < pixels.width; ++x) {
            checksum ^= pixels.getPixelColour(x, y).getARGB();
            checksum *= 1099511628211ULL;
        }
    }

    return checksum;
}

TEST_CASE("Signal probe rail reserves editor-safe workspace bounds", "[cycle-v2][canvas][probe]") {
    const Rectangle<float> workspace { 0.f, 0.f, 1200.f, 800.f };
    SignalProbeRailState expanded;
    expanded.expandedHeight = 190.f;

    const Rectangle<float> content = SignalProbeRail::contentBoundsFor(workspace, expanded);
    REQUIRE(content == Rectangle<float>(0.f, 0.f, 1200.f, 610.f));
    REQUIRE(SignalProbeRail::boundsFor(workspace, expanded).getY() == content.getBottom());
    REQUIRE(SignalProbeRail::collapseHandleFor(workspace, expanded).getY()
            < SignalProbeRail::boundsFor(workspace, expanded).getY());
    REQUIRE(SignalProbeRail::collapseHandleFor(workspace, expanded).getBottom()
            < SignalProbeRail::boundsFor(workspace, expanded).getY() + 24.f);
    REQUIRE(SignalProbeRail::tileBoundsFor(workspace, expanded, 0).getY()
            < SignalProbeRail::boundsFor(workspace, expanded).getY() + 20.f);

    GraphNodeFactory factory;
    const Node trimesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Rectangle<float> editor = NodeCanvasEditorCoordinator::boundsFor(&trimesh, content);
    REQUIRE(content.contains(editor));
    REQUIRE(editor.getBottom() <= content.getBottom());
    REQUIRE(editor.getWidth() == Catch::Approx(content.getWidth() * 0.81f));
    REQUIRE(editor.getHeight() == Catch::Approx(content.getHeight() - 36.f));

    expanded.expanded = false;
    REQUIRE(SignalProbeRail::contentBoundsFor(workspace, expanded).getHeight() == 772.f);
}

}

TEST_CASE("Node palette resolves every authored node kind from its visible entry",
        "[cycle-v2][canvas][palette]") {
    NodePalette palette;

    for (int sectionIndex = 0; sectionIndex < palette.sectionCount(); ++sectionIndex) {
        const auto& section = palette.section(sectionIndex);
        REQUIRE(palette.updateHover(palette.groupBounds(sectionIndex).getCentre()));
        REQUIRE(palette.activeSection() == sectionIndex);

        for (int entryIndex = 0; entryIndex < section.entryCount; ++entryIndex) {
            NodeKind resolvedKind {};
            REQUIRE(palette.findKindAt(palette.entryBounds(sectionIndex, entryIndex).getCentre(), resolvedKind));
            REQUIRE(resolvedKind == section.entries[entryIndex].kind);
        }
    }
}

TEST_CASE("Node palette hover remains open across its pullout and closes outside",
        "[cycle-v2][canvas][palette]") {
    NodePalette palette;
    const int sourceSection = 3;
    const int adjacentSection = 4;

    REQUIRE(palette.updateHover(palette.groupBounds(sourceSection).getCentre()));
    REQUIRE_FALSE(palette.updateHover(palette.entryBounds(sourceSection, 0).getCentre()));
    REQUIRE(palette.activeSection() == sourceSection);

    REQUIRE(palette.updateHover(palette.groupBounds(adjacentSection).getCentre()));
    REQUIRE(palette.activeSection() == adjacentSection);

    REQUIRE(palette.updateHover({ 800.f, 700.f }));
    REQUIRE(palette.activeSection() == -1);
    REQUIRE_FALSE(palette.close());
}

TEST_CASE("Node canvas viewport transforms round trip and preserve zoom anchors", "[cycle-v2][canvas]") {
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1200.f, 800.f });
    viewport.setTransform({ 34.f, 38.f }, 0.58f);
    const Point<float> world { 372.f, 218.f };

    REQUIRE(viewport.toWorld(viewport.toScreen(world)).x == Catch::Approx(world.x));
    REQUIRE(viewport.toWorld(viewport.toScreen(world)).y == Catch::Approx(world.y));

    const Point<float> anchor { 640.f, 360.f };
    const Point<float> before = viewport.toWorld(anchor);
    viewport.zoomAround(anchor, 1.4f);
    const Point<float> after = viewport.toWorld(anchor);
    REQUIRE(after.x == Catch::Approx(before.x));
    REQUIRE(after.y == Catch::Approx(before.y));
    REQUIRE(viewport.centreWorld() == viewport.toWorld(Point<float>(600.f, 400.f)));
}

TEST_CASE("Node canvas viewport snapping is deterministic", "[cycle-v2][canvas]") {
    NodeCanvasViewport viewport;
    REQUIRE(viewport.snap({ 24.f, 26.f }, 10.f) == Point<float>(20.f, 30.f));
    REQUIRE(viewport.snap({ 24.f, 26.f }, 0.f) == Point<float>(24.f, 26.f));
}

TEST_CASE("Node canvas scene shares geometry with typed hit testing", "[cycle-v2][canvas]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 100.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 500.f, 80.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    NodeCanvasViewport viewport;
    viewport.setTransform({ 20.f, 30.f }, 1.f);
    NodeCanvasScene sceneBuilder;
    const auto& scene = sceneBuilder.build(graph, viewport);
    const auto outputTarget = std::find_if(scene.targets.begin(), scene.targets.end(), [](const auto& target) {
        return target.semanticId == "output:wave.out";
    });
    REQUIRE(outputTarget != scene.targets.end());

    const auto hit = NodeCanvasHitTester().hitTest(scene, outputTarget->bounds.getCentre());
    REQUIRE(hit.has_value());
    REQUIRE(hit->kind == NodeSceneTargetKind::OutputPort);
    REQUIRE(hit->portAddress().nodeId == "wave");
    REQUIRE(hit->portAddress().portId == "out");
}

TEST_CASE("Node canvas scene invalidates only for relevant revisions", "[cycle-v2][canvas]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    NodeCanvasViewport viewport;
    NodeCanvasScene scene;

    const auto* firstTargets = scene.build(graph, viewport, 4).targets.data();
    const auto* cachedTargets = scene.build(graph, viewport, 4).targets.data();
    REQUIRE(cachedTargets == firstTargets);

    viewport.panBy({ 1.f, 0.f });
    REQUIRE(scene.build(graph, viewport, 4).viewportRevision == viewport.getRevision());
    REQUIRE(scene.build(graph, viewport, 5).presentationRevision == 5);
    REQUIRE(scene.build(graph, viewport, 5, 99).graphRevision == 99);
}

TEST_CASE("Graph document rejects failed loads without replacing active state", "[cycle-v2][canvas][document]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    const String before = document.toXml();

    REQUIRE_FALSE(document.loadXml("<not-a-graph/>", true));
    REQUIRE(document.toXml() == before);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Graph command dispatcher records semantic edits and undo", "[cycle-v2][canvas][document]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    const size_t initialNodeCount = document.graph().getNodes().size();
    const uint64_t initialRevision = document.revision();

    const auto added = commands.addNode(NodeKind::Envelope, { 100.f, 140.f });
    REQUIRE(added.succeeded());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount + 1);
    REQUIRE(document.revision() == initialRevision + 1);
    REQUIRE(document.lastChange().topologyChanged);
    REQUIRE(document.canUndo());

    REQUIRE(document.undo());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount);
    REQUIRE(document.redo());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount + 1);
}

TEST_CASE("Graph command dispatcher coalesces a drag into one undo entry", "[cycle-v2][canvas][document]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 10.f, 20.f }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    commands.beginCompoundEdit();
    REQUIRE(commands.moveNode("env", { 40.f, 50.f }).succeeded());
    REQUIRE(commands.moveNode("env", { 80.f, 90.f }).succeeded());
    commands.commitCompoundEdit();
    REQUIRE(document.graph().findNode("env")->bounds.getPosition() == Point<float>(80.f, 90.f));

    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("env")->bounds.getPosition() == Point<float>(10.f, 20.f));
    REQUIRE_FALSE(document.undo());
}

TEST_CASE("Canvas and automation command requests share the same dispatcher", "[cycle-v2][canvas][document]") {
    GraphDocument canvasDocument(NodeGraph::createDemoGraph());
    GraphDocument automationDocument(NodeGraph::createDemoGraph());
    GraphCommandDispatcher canvasCommands(canvasDocument);
    GraphCommandDispatcher automationCommands(automationDocument);

    REQUIRE(canvasCommands.setNodeParameter("voice", "voices", "Voices", "4").succeeded());
    REQUIRE(automationCommands.setNodeParameter("voice", "voices", "Voices", "4").succeeded());
    REQUIRE(canvasDocument.toXml() == automationDocument.toXml());
}

TEST_CASE("Graph presentation schedules work from semantic change impacts", "[cycle-v2][canvas][presentation]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(graph, 1, topology));
    const size_t compilationCount = presentation.compilationCount();
    const size_t previewCount = presentation.previewRenderCount();

    GraphChangeSet layout;
    layout.layoutChanged = true;
    REQUIRE(presentation.refresh(graph, 2, layout));
    REQUIRE(presentation.compilationCount() == compilationCount);
    REQUIRE(presentation.previewRenderCount() == previewCount);

    GraphChangeSet preview;
    preview.parameterImpacts = ParameterImpact::Preview;
    REQUIRE(presentation.refresh(graph, 3, preview));
    REQUIRE(presentation.compilationCount() == compilationCount);
    REQUIRE(presentation.previewRenderCount() == previewCount + 1);
}

TEST_CASE("Graph presentation rejects stale revision results", "[cycle-v2][canvas][presentation]") {
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(NodeGraph::createDemoGraph(), 7, topology));

    GraphPresentationSnapshot stale;
    stale.graphRevision = 6;
    REQUIRE_FALSE(presentation.acceptSnapshot(std::move(stale)));
    REQUIRE(presentation.snapshot().graphRevision == 7);
}

TEST_CASE("Graph presentation preserves configuration revision history across recompiles",
        "[cycle-v2][canvas][presentation]") {
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    NodeGraph graph = GraphSerializer().fromXmlString(defaultGraph.loadFileAsString());
    GraphNodeFactory factory;
    graph.replaceNodeParameters("waveshaper",
            factory.createNode(NodeKind::Waveshaper, "defaults", {}).parameters);
    graph.replaceNodeParameters("ir",
            factory.createNode(NodeKind::ImpulseResponse, "defaults", {}).parameters);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(document.graph(), document.revision(), topology));
    REQUIRE(presentation.compileResult().succeeded());
    const auto initialAudio = presentation.captureAudio(document.graph(), 128).output.block.samples;
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples == initialAudio);

    const auto revisionFor = [&](const String& nodeId) {
        const auto& steps = presentation.compileResult().plan.steps;
        String stepIds;
        for (const auto& step : steps) {
            stepIds << step.nodeId << " ";
        }
        const auto found = std::find_if(steps.begin(), steps.end(), [&](const auto& step) {
            return step.nodeId == nodeId;
        });
        INFO("configuration revision requested for " << nodeId << "; steps: " << stepIds);
        REQUIRE(found != steps.end());
        return found->configuration.revision;
    };

    const uint64_t initialRevision = revisionFor("waveshaper");
    const uint64_t initialIrRevision = revisionFor("ir");
    const uint64_t initialEnvelopeRevision = revisionFor("env");
    REQUIRE(commands.setNodeParameter("waveshaper", "pre", "Pre", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("waveshaper") == initialRevision + 1);
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples != initialAudio);

    REQUIRE(commands.setNodeParameter("ir", "post", "Post", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("ir") == initialIrRevision + 1);
    const auto irAudio = presentation.captureAudio(document.graph(), 128).output.block.samples;

    REQUIRE(commands.setNodeParameter("env", "red", "Red", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("env") == initialEnvelopeRevision + 1);
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples != irAudio);
}

TEST_CASE("Rich node views are selected through the view module registry", "[cycle-v2][canvas][view]") {
    const auto& registry = NodeViewModuleRegistry::instance();
    REQUIRE(registry.moduleFor(NodeKind::Envelope).capabilities().hostedEditor);
    REQUIRE(registry.moduleFor(NodeKind::TrilinearMesh).capabilities().outputSideControl);
    REQUIRE(registry.moduleFor(NodeKind::Add).capabilities().operationLayoutControl);
    REQUIRE_FALSE(registry.moduleFor(NodeKind::Output).capabilities().hostedEditor);
    REQUIRE(registry.moduleFor(NodeKind::Envelope).editorFactory() != nullptr);
    REQUIRE(registry.moduleFor(NodeKind::TrilinearMesh).editorFactory() != nullptr);
    REQUIRE(registry.moduleFor(NodeKind::Output).editorFactory() == nullptr);

    const auto bounds = registry.moduleFor(NodeKind::ImpulseResponse)
            .expandedEditorBounds({ 0.f, 0.f, 1400.f, 800.f }, 18.f);
    REQUIRE(bounds.getWidth() == Catch::Approx(1050.f));
    REQUIRE(bounds.getHeight() == Catch::Approx(470.f));

    const auto meshBounds = registry.moduleFor(NodeKind::TrilinearMesh)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(meshBounds.getWidth() == Catch::Approx(1080.f));
    REQUIRE(meshBounds.getHeight() == Catch::Approx(720.f));
}

TEST_CASE("Registered view modules contribute dynamic attachment geometry", "[cycle-v2][canvas][scene]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GuideCurve, "guide", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 420.f, 80.f }));
    graph.addEdge({ "guide", "guide", "mesh", "guide.vertex.0.red",
            PortDomain::ControlSignal, true });

    NodeCanvasViewport viewport;
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().destination.y
            == Catch::Approx(viewport.toScreen(graph.findNode("mesh")->bounds.getTopLeft()).y));
    REQUIRE_FALSE(snapshot.edges.front().destinationPortLike);
    REQUIRE(snapshot.edges.front().cablePath.getBounds().expanded(0.1f)
            .contains(snapshot.edges.front().source));
    REQUIRE(snapshot.edges.front().cablePath.getBounds().expanded(0.1f)
            .contains(snapshot.edges.front().destination));
    REQUIRE(snapshot.edges.front().hitPath.contains(
            snapshot.edges.front().cablePath.getPointAlongPath(
                    snapshot.edges.front().cablePath.getLength() * 0.5f)));
}

TEST_CASE("Cable renderer exposes ordinary attachment and edit-state semantics",
        "[cycle-v2][canvas][cables]") {
    NodeSceneEdge edge;
    edge.source = { 30.f, 50.f };
    edge.destination = { 210.f, 130.f };
    edge.cablePath = NodeCanvasScene::cablePath(
            edge.source,
            edge.destination,
            PortSide::Right,
            PortSide::Left,
            1.f);

    const std::array<NodeCableStyle, 5> styles {
            NodeCableStyle { Colour(0xff42d3cf), false, false, false, false },
            NodeCableStyle { Colour(0xff42d3cf), true, false, false, false },
            NodeCableStyle { Colour(0xffff5a5f), false, true, false, false },
            NodeCableStyle { Colour(0xff42d3cf), false, false, true, false },
            NodeCableStyle { Colour(0xff42d3cf), false, false, false, true }
    };
    std::array<uint64_t, styles.size()> checksums {};

    for (size_t i = 0; i < styles.size(); ++i) {
        Image image(Image::ARGB, 240, 180, true);
        Graphics graphics(image);
        NodeCableRenderer::paint(graphics, edge, styles[i], 1.f);
        checksums[i] = imageChecksum(image);
        REQUIRE(checksums[i] != imageChecksum(Image(Image::ARGB, 240, 180, true)));
    }

    for (size_t i = 0; i < checksums.size(); ++i) {
        for (size_t j = i + 1; j < checksums.size(); ++j) {
            REQUIRE(checksums[i] != checksums[j]);
        }
    }
}

TEST_CASE("Voice context editor resolves every authored control from its painted rows",
        "[cycle-v2][canvas][compact-editor]") {
    Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});
    const Rectangle<float> panel { 0.f, 0.f, 700.f, 400.f };

    auto editAt = [&](Point<float> point) {
        const auto edit = VoiceContextCompactEditor::editAt(voice, panel, point);
        REQUIRE(edit.has_value());
        return *edit;
    };

    REQUIRE(VoiceContextCompactEditor::domainLabel(voice) == "Waveform");
    REQUIRE(VoiceContextCompactEditor::nextDomain(voice) == "spectral");

    auto edit = editAt({ 252.f, 59.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Domain);
    REQUIRE(edit.value == "spectral");

    edit = editAt({ 108.f, 85.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Octave);
    REQUIRE(edit.value == "-2");
    REQUIRE(editAt({ 389.f, 85.5f }).value == "0");
    REQUIRE(editAt({ 670.f, 85.5f }).value == "2");

    edit = editAt({ 106.f, 111.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Pitch);
    REQUIRE(edit.value == "-12");
    REQUIRE(editAt({ 389.f, 111.5f }).value == "0");
    REQUIRE(editAt({ 672.f, 111.5f }).value == "12");

    edit = editAt({ 112.f, 137.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Portamento);
    REQUIRE(edit.value == "1");

    edit = editAt({ 106.f, 163.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Oversampling);
    REQUIRE(edit.value == "1x");
    REQUIRE(editAt({ 389.f, 163.5f }).value == "2x");
    REQUIRE(editAt({ 672.f, 163.5f }).value == "4x");

    const Rectangle<float> selector = VoiceContextCompactEditor::nodeSelectorBounds(
            voice.bounds,
            1.f);
    REQUIRE(VoiceContextCompactEditor::hitNodeSelector(
            voice.bounds,
            1.f,
            selector.getCentre()));
}

TEST_CASE("Transform editor exposes FFT and IFFT mode semantics through one geometry contract",
        "[cycle-v2][canvas][compact-editor]") {
    GraphNodeFactory factory;
    const Rectangle<float> panel { 0.f, 0.f, 700.f, 400.f };
    const Point<float> left { 245.f, 61.f };
    const Point<float> right { 535.f, 61.f };
    Node fft = factory.createNode(NodeKind::Fft, "fft", {});
    Node ifft = factory.createNode(NodeKind::Ifft, "ifft", {});

    REQUIRE(TransformCompactEditor::modeAt(fft, panel, left) == TransformMode::Cycle);
    REQUIRE(TransformCompactEditor::modeAt(fft, panel, right) == TransformMode::FixedWindow);
    REQUIRE(TransformCompactEditor::parameterValue(TransformMode::FixedWindow) == "fixedWindow");
    REQUIRE(TransformCompactEditor::subtitle(NodeKind::Fft, TransformMode::FixedWindow) == "fixed window");
    REQUIRE(TransformCompactEditor::status(NodeKind::Fft, TransformMode::Cycle)
            == "Time to freq: chunked by cycle");

    REQUIRE(TransformCompactEditor::modeAt(ifft, panel, left) == TransformMode::Cyclic);
    REQUIRE(TransformCompactEditor::modeAt(ifft, panel, right) == TransformMode::AcyclicCarry);
    REQUIRE(TransformCompactEditor::parameterValue(TransformMode::AcyclicCarry) == "acyclicCarry");
    REQUIRE(TransformCompactEditor::subtitle(NodeKind::Ifft, TransformMode::AcyclicCarry)
            == "carry overlap");
    REQUIRE(TransformCompactEditor::status(NodeKind::Ifft, TransformMode::Cyclic)
            == "Freq to time: cyclic overlap");
}
