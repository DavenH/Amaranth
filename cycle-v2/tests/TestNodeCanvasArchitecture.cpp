#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <set>

#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Graph/NodeDefinition.h"
#include "../src/UI/EnvelopePurposeIconRenderer.h"
#include "../src/UI/EnvelopePurposeSelector.h"
#include "../src/UI/NodeCanvasScene.h"
#include "../src/UI/NodeCanvasEditorCoordinator.h"
#include "../src/UI/NodeCanvasPresentation.h"
#include "../src/UI/NodeCableRenderer.h"
#include "../src/UI/NodeCanvasViewport.h"
#include "../src/UI/NodePalette.h"
#include "../src/UI/NodePaletteEntryIconRenderer.h"
#include "../src/UI/NodePreviewRenderer.h"
#include "../src/UI/NodeViewModule.h"
#include "../src/UI/SignalProbeRail.h"
#include "../src/UI/TransformCompactEditor.h"
#include "../src/UI/VoiceContextCompactEditor.h"
#include "../src/Runtime/GraphPresentationModel.h"

using namespace CycleV2;

TEST_CASE("EQ response preview does not require an Effect2D model",
        "[cycle-v2][canvas][equalizer][regression]") {
    REQUIRE_FALSE(NodePreviewRenderer::requiresEffect2DModel(NodeKind::Equalizer));
    REQUIRE(NodePreviewRenderer::requiresEffect2DModel(NodeKind::Envelope));
    REQUIRE(NodePreviewRenderer::requiresEffect2DModel(NodeKind::Waveshaper));
}

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
    const Rectangle<float> collapse = SignalProbeRail::collapseHandleFor(workspace, expanded);
    const Rectangle<float> refreshMode = SignalProbeRail::refreshModeBoundsFor(workspace, expanded);
    const Rectangle<float> rail = SignalProbeRail::boundsFor(workspace, expanded);
    REQUIRE(collapse.getBottom() <= rail.getY());
    REQUIRE(refreshMode.getBottom() <= rail.getY());
    REQUIRE_FALSE(collapse.intersects(refreshMode));
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

TEST_CASE("Spectral preview excludes DC and preserves low harmonic detail",
        "[cycle-v2][canvas][preview][spectral]") {
    constexpr size_t columns = 2;
    constexpr size_t rows = 65;
    std::vector<float> withDc(columns * rows);
    std::vector<float> withoutDc(columns * rows);

    for (size_t column = 0; column < columns; ++column) {
        withDc[column * rows] = 1000.f;

        for (size_t harmonic = 1; harmonic < rows; ++harmonic) {
            const float magnitude = 1.f / (float) harmonic;
            withDc[column * rows + harmonic] = magnitude;
            withoutDc[column * rows + harmonic] = magnitude;
        }
    }

    const auto profile = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal);
    const auto mappedWithDc = profile.mapGridToDisplay(
            withDc,
            columns,
            rows);
    const auto mappedWithoutDc = profile.mapGridToDisplay(
            withoutDc,
            columns,
            rows);

    REQUIRE(mappedWithDc == mappedWithoutDc);
    REQUIRE(mappedWithDc[0] > mappedWithDc[rows / 2]);
    REQUIRE(mappedWithDc[rows / 2] > mappedWithDc[rows - 1]);
    REQUIRE(mappedWithDc[rows / 8] > 0.1f);
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

TEST_CASE("Every registered node kind has a parseable palette icon",
        "[cycle-v2][canvas][palette][icons]") {
    for (const auto& definition : NodeDefinitionRegistry::instance().definitions()) {
        INFO("Missing or invalid icon for node type " << definition.typeId);
        REQUIRE(NodePaletteEntryIconRenderer::hasIcon(definition.kind));
    }
}

TEST_CASE("Every Envelope purpose has a parseable compact icon",
        "[cycle-v2][canvas][envelope][icons]") {
    Image blank(Image::ARGB, 24, 24, true);
    const uint64_t blankChecksum = imageChecksum(blank);
    std::set<uint64_t> checksums;

    for (const EnvelopePurpose purpose : kEnvelopePurposes) {
        INFO("Missing or invalid Envelope purpose icon for "
                << envelopePurposeToString(purpose));
        REQUIRE(EnvelopePurposeIconRenderer::hasIcon(purpose));

        Image rendered(Image::ARGB, 24, 24, true);
        Graphics graphics(rendered);
        EnvelopePurposeIconRenderer::paint(
                graphics,
                purpose,
                rendered.getBounds().toFloat());
        const uint64_t checksum = imageChecksum(rendered);
        REQUIRE(checksum != blankChecksum);
        REQUIRE(checksums.emplace(checksum).second);
    }
}

TEST_CASE("Envelope mode selector presents one contiguous highlighted choice",
        "[cycle-v2][canvas][envelope][icons][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    MessageManagerLock messageLock;
    REQUIRE(messageLock.lockWasGained());
    EnvelopePurposeSelector selector;
    selector.setBounds(0, 0, 148, 28);
    int changes {};
    selector.onChange = [&changes](EnvelopePurpose) {
        ++changes;
    };

    std::set<uint64_t> selectedChecksums;
    Rectangle<float> previous;
    for (const EnvelopePurpose purpose : kEnvelopePurposes) {
        selector.setPurpose(purpose, sendNotificationSync);
        REQUIRE(selector.purpose() == purpose);
        const auto bounds = selector.optionBounds(purpose);
        REQUIRE(bounds.getHeight() == Catch::Approx(28.f));
        if (!previous.isEmpty()) {
            REQUIRE(bounds.getX() == Catch::Approx(previous.getRight()));
        }
        previous = bounds;

        const Image rendered = selector.createComponentSnapshot(selector.getLocalBounds());
        REQUIRE(selectedChecksums.emplace(imageChecksum(rendered)).second);
    }

    REQUIRE(selector.getNumChildComponents() == 4);
    REQUIRE(changes == 3);
    selector.setPurpose(EnvelopePurpose::Scratch, sendNotificationSync);
    REQUIRE(changes == 3);
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
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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
    const auto& documentChanged = scene.build(graph, viewport, 5, 99);
    REQUIRE(documentChanged.graphRevision == graph.getRevision());
    REQUIRE(documentChanged.documentRevision == 99);
}

TEST_CASE("Graph document rejects failed loads without replacing active state", "[cycle-v2][canvas][document]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    const String before = document.toJson();

    REQUIRE_FALSE(document.loadJson("<not-a-graph/>", true));
    REQUIRE(document.toJson() == before);
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

    REQUIRE(canvasCommands.setNodeParameter("voice", "pitch", "Pitch", "4").succeeded());
    REQUIRE(automationCommands.setNodeParameter("voice", "pitch", "Pitch", "4").succeeded());
    REQUIRE(canvasDocument.toJson() == automationDocument.toJson());
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

TEST_CASE("Typed model edits refresh configuration without topology compilation",
        "[cycle-v2][canvas][presentation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(document.graph(), document.revision(), topology));
    const size_t compilationCount = presentation.compilationCount();
    const uint64_t configurationRevision =
            presentation.compileResult().plan.steps.front().configuration.revision;

    const auto current = std::dynamic_pointer_cast<const CurveNodeModelState>(
            document.graph().findNode("shape")->model);
    FlatCurveModel edited;
    REQUIRE(current != nullptr);
    REQUIRE(current->flatCurve() != nullptr);
    REQUIRE(edited.copyFrom(*current->flatCurve()));
    auto vertices = edited.getVertices();
    vertices.front().y += 0.01f;
    REQUIRE(edited.replaceVertices(std::move(vertices)));
    REQUIRE(commands.replaceNodeModel(
            "shape",
            current->revision(),
            CurveNodeModelState::copyOf(edited, current->revision() + 1)).succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(presentation.compilationCount() == compilationCount);
    REQUIRE(presentation.compileResult().plan.steps.front().configuration.revision
            == configurationRevision + 1);
}

TEST_CASE("Graph presentation preserves configuration revision history across recompiles",
        "[cycle-v2][canvas][presentation]") {
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    NodeGraph graph = GraphSerializer().fromJsonString(defaultGraph.loadFileAsString());
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
    REQUIRE(meshBounds.getWidth() == Catch::Approx(972.f));
    REQUIRE(meshBounds.getHeight() == Catch::Approx(764.f));

    const auto modulationBounds = registry.moduleFor(NodeKind::ModulationSource)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(modulationBounds.getWidth() == Catch::Approx(260.f));
    REQUIRE(modulationBounds.getHeight() == Catch::Approx(116.f));

    const auto tripleBounds = registry.moduleFor(NodeKind::ModulationTriple)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(tripleBounds.getWidth() == Catch::Approx(370.f));
    REQUIRE(tripleBounds.getHeight() == Catch::Approx(230.f));
}

TEST_CASE("Every effect view exposes both its compact preview and hosted editor",
        "[cycle-v2][canvas][view][effects]") {
    const auto& registry = NodeViewModuleRegistry::instance();
    for (const NodeKind kind : {
            NodeKind::Unison,
            NodeKind::Reverb,
            NodeKind::Delay,
            NodeKind::Equalizer }) {
        const auto& module = registry.moduleFor(kind);
        REQUIRE(module.capabilities().previewable);
        REQUIRE(module.capabilities().hostedEditor);
        REQUIRE(module.editorFactory() != nullptr);
    }

    const auto reverbBounds = registry.moduleFor(NodeKind::Reverb)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(reverbBounds.getWidth() == Catch::Approx(520.f));
    REQUIRE(reverbBounds.getHeight() == Catch::Approx(520.f));

    const auto delayBounds = registry.moduleFor(NodeKind::Delay)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(delayBounds.getWidth() == Catch::Approx(520.f));
    REQUIRE(delayBounds.getHeight() == Catch::Approx(520.f));
}

TEST_CASE("Registered view modules contribute dynamic attachment geometry", "[cycle-v2][canvas][scene]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GuideCurve, "guide", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 420.f, 80.f }));
    graph.addEdge({ "guide", "guide", "mesh", "guide.cube.0.red",
            PortDomain::ControlSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::GuideCurve });

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

TEST_CASE("Cube-component assignments share one attachment cable per node pair",
        "[cycle-v2][canvas][scene][attachments]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GuideCurve, "guide", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 420.f, 80.f }));
    graph.addEdge({ "guide", "guide", "mesh", "guide.cube.0.time",
            PortDomain::ControlSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::GuideCurve });
    graph.addEdge({ "guide", "guide", "mesh", "guide.cube.0.amp",
            PortDomain::ControlSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::GuideCurve });

    NodeCanvasViewport viewport;
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().edgeIndices == std::vector<int> { 0, 1 });
    REQUIRE_FALSE(snapshot.edges.front().modulationBundle);
}

TEST_CASE("Cable endpoints follow node movement before a drag transaction commits",
        "[cycle-v2][canvas][scene][cables]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "source", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 420.f, 80.f }));
    graph.addEdge({
            "source", "out", "output", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal });

    NodeCanvasViewport viewport;
    NodeCanvasScene scene;
    constexpr uint64_t presentationRevision = 7;
    constexpr uint64_t documentRevision = 11;
    const auto initialDestination = scene.build(
            graph, viewport, presentationRevision, documentRevision)
            .edges.front().destination;

    REQUIRE(graph.setNodeBounds(
            "output",
            graph.findNode("output")->bounds.withPosition({ 560.f, 190.f })));
    const auto& moved = scene.build(
            graph, viewport, presentationRevision, documentRevision);

    REQUIRE(moved.edges.front().destination != initialDestination);
    REQUIRE(moved.edges.front().destination == viewport.toScreen(
            NodeCanvasScene::portWorldCentre(
                    *graph.findNode("output"), graph.findNode("output")->inputs.front())));
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
    REQUIRE(VoiceContextCompactEditor::summaryLabel(voice, 1.0)
            == "Octave 0  ·  1 second");
    REQUIRE(VoiceContextCompactEditor::summaryLabel(voice, 0.25)
            == "Octave 0  ·  0.25 seconds");

    voice.parameters = {
            { "domain", "Start Domain", "spectralPhase" }
    };
    REQUIRE(VoiceContextCompactEditor::domainLabel(voice) == "Spectral");
    REQUIRE(VoiceContextCompactEditor::nextDomain(voice) == "waveform");
    voice.parameters.clear();

    voice.parameters = {
            { "octave", "Octave", "1" },
            { "pitch", "Pitch", "-5" },
            { "portamento", "Portamento", "1" },
            { "oversampling", "Oversampling", "4x" }
    };
    REQUIRE(VoiceContextCompactEditor::summaryLabel(voice, 2.0)
            == "Octave 1  ·  2 seconds  ·  Glide");
    voice.parameters.clear();

    auto edit = editAt({ 252.f, 59.5f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Domain);
    REQUIRE(edit.value == "spectral");

    const Rectangle<float> octave = VoiceContextCompactEditor::octaveControlBounds(panel);
    edit = editAt({ octave.getX(), octave.getCentreY() });
    REQUIRE(edit.control == VoiceContextEdit::Control::Octave);
    REQUIRE(edit.value == "-2");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Octave,
            panel,
            octave.getCentreX())->value == "0");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Octave,
            panel,
            octave.getRight())->value == "2");

    const Rectangle<float> voiceLength = VoiceContextCompactEditor::voiceLengthControlBounds(panel);
    edit = editAt({ voiceLength.getCentreX(), voiceLength.getCentreY() });
    REQUIRE(edit.control == VoiceContextEdit::Control::VoiceLength);
    REQUIRE(VoiceContextCompactEditor::voiceLengthAt(panel, voiceLength.getX())
            == Catch::Approx(std::exp(-3.0)));
    REQUIRE(VoiceContextCompactEditor::voiceLengthAt(panel, voiceLength.getRight())
            == Catch::Approx(std::exp(5.0)));

    const Rectangle<float> pitch = VoiceContextCompactEditor::pitchControlBounds(panel);
    edit = editAt({ pitch.getX(), pitch.getCentreY() });
    REQUIRE(edit.control == VoiceContextEdit::Control::Pitch);
    REQUIRE(edit.value == "-12");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Pitch,
            panel,
            pitch.getCentreX())->value == "0");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Pitch,
            panel,
            pitch.getRight())->value == "12");

    const Rectangle<float> oversampling =
            VoiceContextCompactEditor::oversamplingControlBounds(panel);
    edit = editAt({ oversampling.getX(), oversampling.getCentreY() });
    REQUIRE(edit.control == VoiceContextEdit::Control::Oversampling);
    REQUIRE(edit.value == "1x");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Oversampling,
            panel,
            oversampling.getCentreX())->value == "4x");
    REQUIRE(VoiceContextCompactEditor::sliderEditAt(
            VoiceContextEdit::Control::Oversampling,
            panel,
            oversampling.getRight())->value == "8x");

    REQUIRE(octave.getWidth() == Catch::Approx(voiceLength.getWidth()));
    REQUIRE(octave.getWidth() == Catch::Approx(pitch.getWidth()));
    REQUIRE(octave.getWidth() == Catch::Approx(oversampling.getWidth()));

    edit = editAt({ 112.f, 237.f });
    REQUIRE(edit.control == VoiceContextEdit::Control::Portamento);
    REQUIRE(edit.value == "1");

    const Rectangle<float> selector = VoiceContextCompactEditor::nodeSelectorBounds(
            voice.bounds,
            1.f);
    REQUIRE(VoiceContextCompactEditor::hitNodeSelector(
            voice.bounds,
            1.f,
            selector.getCentre()));
}

TEST_CASE("Shared Unison preview does not depend on attachment edge order",
        "[cycle-v2][canvas][voice-context][unison]") {
    GraphExecutionPlan plan;
    plan.configurationAttachments = {
            { "unison", "unison", "first", "unison", PortDomain::VoiceControlSignal,
                    ConnectionKind::ConfigurationAttachment, AttachmentType::Unison },
            { "unison", "unison", "second", "unison", PortDomain::VoiceControlSignal,
                    ConnectionKind::ConfigurationAttachment, AttachmentType::Unison }
    };
    CompiledVoiceContext first;
    first.nodeId = "first";
    first.pitchEnvelopeUnitValues = { 0.25f, 0.5f };
    CompiledVoiceContext second;
    second.nodeId = "second";
    second.pitchEnvelopeUnitValues = { 0.75f, 1.f };
    plan.voiceContexts = { first, second };
    const UnisonPreviewContext fallback { 60, 1.0, { 0.5f } };

    REQUIRE(NodeCanvasPresentation::unisonPreviewContextFor(
            plan, "unison", fallback).pitchEnvelopeUnitValues
            == fallback.pitchEnvelopeUnitValues);
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
