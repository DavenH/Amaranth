#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <set>

#include <App/AppConstants.h>
#include <Util/Arithmetic.h>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Graph/NodeDefinition.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EnvelopePurposeIconRenderer.h"
#include "UI/EnvelopePurposeSelector.h"
#include "UI/EffectEnableButton.h"
#include "UI/EditorChromeLayout.h"
#include "UI/GuideRelationshipPresentation.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodeCanvasEditorCoordinator.h"
#include "UI/NodeCanvasPresentation.h"
#include "UI/NodeCableRenderer.h"
#include "UI/NodeCanvasViewport.h"
#include "UI/NodeIconRenderer.h"
#include "UI/NodePalette.h"
#include "UI/NodePaletteEntryIconRenderer.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodeViewModule.h"
#include "UI/SignalProbeDetailView.h"
#include "UI/SignalProbeRail.h"
#include "UI/TransformCompactEditor.h"
#include "UI/VoiceContextCompactEditor.h"
#include "UI/WorkspaceDock.h"
#include "UI/WorkspaceDockKeyboardNavigation.h"
#include "Runtime/GraphPresentationModel.h"
#include "Runtime/PreviewPitchResolver.h"

using namespace CycleV2;

TEST_CASE("EQ response preview does not require a Curve model",
        "[cycle-v2][canvas][equalizer][regression]") {
    REQUIRE_FALSE(NodePreviewRenderer::requiresCurveModel(NodeKind::Equalizer));
    REQUIRE(NodePreviewRenderer::requiresCurveModel(NodeKind::Envelope));
    REQUIRE(NodePreviewRenderer::requiresCurveModel(NodeKind::Waveshaper));
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
    REQUIRE(rail.contains(collapse));
    REQUIRE(rail.contains(refreshMode));
    REQUIRE_FALSE(collapse.intersects(refreshMode));
    REQUIRE(SignalProbeRail::tileBoundsFor(workspace, expanded, 0).getY()
            == Catch::Approx(SignalProbeRail::boundsFor(workspace, expanded).getY()
                    + WorkspaceDock::headerHeight));

    GraphNodeFactory factory;
    const Node trimesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Rectangle<float> editor = NodeCanvasEditorCoordinator::boundsFor(&trimesh, content);
    REQUIRE(content.contains(editor));
    REQUIRE(editor.getBottom() <= content.getBottom());
    REQUIRE(editor.getWidth() == Catch::Approx(content.getWidth() * 0.81f));
    REQUIRE(editor.getHeight() == Catch::Approx(content.getHeight() - 36.f));

    expanded.expanded = false;
    REQUIRE(SignalProbeRail::contentBoundsFor(workspace, expanded).getHeight()
            == 800.f - WorkspaceDock::collapsedHeight);
}

TEST_CASE("Workspace dock is the single clamped Guide and Spy layout authority",
        "[cycle-v2][canvas][guide-dock]") {
    const Rectangle<float> workspace { 0.f, 0.f, 1000.f, 700.f };
    WorkspaceDockState state;
    const WorkspaceDockLayout balanced = WorkspaceDock::layout(workspace, state);

    REQUIRE(balanced.content.getBottom() == balanced.dock.getY());
    REQUIRE(balanced.leftShelf.getWidth() == Catch::Approx(500.f));
    REQUIRE(balanced.rightShelf.getWidth() == Catch::Approx(500.f));
    REQUIRE(balanced.leftShelf.getRight() == Catch::Approx(balanced.rightShelf.getX()));
    REQUIRE(balanced.dock.contains(balanced.collapseHandle));
    REQUIRE(balanced.resizeHandle.getY() == balanced.dock.getY());

    state.splitRatio = 0.05f;
    const WorkspaceDockLayout clamped = WorkspaceDock::layout(workspace, state);
    REQUIRE(clamped.leftShelf.getWidth() == Catch::Approx(WorkspaceDock::minimumShelfWidth));

    state.leftMinimized = true;
    const WorkspaceDockLayout leftDrawer = WorkspaceDock::layout(workspace, state);
    REQUIRE(leftDrawer.leftShelf.getWidth() == Catch::Approx(WorkspaceDock::drawerWidth));
    REQUIRE(leftDrawer.rightShelf.getWidth()
            == Catch::Approx(workspace.getWidth() - WorkspaceDock::drawerWidth));
    REQUIRE(leftDrawer.divider.isEmpty());

    state.leftMinimized = false;
    state.expanded = false;
    const WorkspaceDockLayout collapsed = WorkspaceDock::layout(workspace, state);
    REQUIRE(collapsed.dock.getHeight() == Catch::Approx(WorkspaceDock::collapsedHeight));
    REQUIRE(collapsed.leftShelf.isEmpty());
    REQUIRE(collapsed.resizeHandle.isEmpty());

    const Rectangle<float> smallWorkspace { 0.f, 0.f, 360.f, 400.f };
    state.expanded = true;
    state.splitRatio = 0.8f;
    const WorkspaceDockLayout small = WorkspaceDock::layout(smallWorkspace, state);
    REQUIRE(small.leftShelf.getWidth() == Catch::Approx(180.f));
    REQUIRE(small.rightShelf.getWidth() == Catch::Approx(180.f));
}

TEST_CASE("Workspace dock keyboard traversal exposes every visible action",
        "[cycle-v2][canvas][guide-dock][keyboard]") {
    WorkspaceDockKeyboardModel model;
    model.guideIds = { "guide1", "guide2" };
    model.spyIds = { "probe1" };

    const auto order = WorkspaceDockKeyboardNavigation::focusOrder(model);
    REQUIRE(order.front().target == WorkspaceDockFocusTarget::Collapse);
    REQUIRE(std::count(order.begin(), order.end(), WorkspaceDockFocus {
            WorkspaceDockFocusTarget::GuideTile, "guide1" }) == 1);
    REQUIRE(std::count(order.begin(), order.end(), WorkspaceDockFocus {
            WorkspaceDockFocusTarget::SpyTile, "probe1" }) == 1);

    WorkspaceDockFocus focus;
    REQUIRE(WorkspaceDockKeyboardNavigation::moveFocus(
            KeyPress(KeyPress::tabKey), model, focus));
    REQUIRE(focus.target == WorkspaceDockFocusTarget::Collapse);
    REQUIRE(WorkspaceDockKeyboardNavigation::moveFocus(
            KeyPress(KeyPress::tabKey, ModifierKeys::shiftModifier, 0), model, focus));
    REQUIRE(focus == order.back());

    focus = { WorkspaceDockFocusTarget::GuideTile, "guide1" };
    REQUIRE(WorkspaceDockKeyboardNavigation::moveFocus(
            KeyPress(KeyPress::rightKey), model, focus));
    REQUIRE(focus.itemId == "guide2");
    REQUIRE(WorkspaceDockKeyboardNavigation::moveFocus(
            KeyPress(KeyPress::downKey), model, focus));
    const WorkspaceDockFocus expectedSpy {
            WorkspaceDockFocusTarget::SpyTile,
            "probe1"
    };
    REQUIRE(focus == expectedSpy);

    model.expanded = false;
    const auto collapsedOrder = WorkspaceDockKeyboardNavigation::focusOrder(model);
    REQUIRE(collapsedOrder.size() == 1);
    REQUIRE(collapsedOrder.front().target == WorkspaceDockFocusTarget::Collapse);
}

TEST_CASE("Workspace dock reveals keyboard-focused overflow tiles",
        "[cycle-v2][canvas][guide-dock][keyboard]") {
    const float maximumOffset = 900.f;
    const float first = WorkspaceDock::offsetToRevealTile(420.f, maximumOffset, 500.f, 0);
    const float last = WorkspaceDock::offsetToRevealTile(0.f, maximumOffset, 500.f, 5);

    REQUIRE(first == Catch::Approx(0.f));
    REQUIRE(last > 0.f);
    REQUIRE(last <= maximumOffset);
}

TEST_CASE("Guide relationship selection highlights without drawing a persistent tether",
        "[cycle-v2][canvas][guide-dock][relationship]") {
    GuideCurveShelfState state;
    state.selectedGuideId = "guide1";

    REQUIRE(GuideRelationshipPresentation::highlightGuideId(state) == "guide1");
    REQUIRE(GuideRelationshipPresentation::tetherGuideId(state).isEmpty());

    state.hoveredGuideId = "guide2";
    REQUIRE(GuideRelationshipPresentation::highlightGuideId(state) == "guide2");
    REQUIRE(GuideRelationshipPresentation::tetherGuideId(state) == "guide2");
}

TEST_CASE("Canvas status gives hover help precedence over the last edit",
        "[cycle-v2][canvas][status]") {
    REQUIRE(NodeCanvasPresentation::canvasStatusText("Node added", {}) == "Node added");
    REQUIRE(NodeCanvasPresentation::canvasStatusText(
            "Node added",
            "Time signal from Oscillator to Output.")
            == "Time signal from Oscillator to Output.");
}

TEST_CASE("Guide relationship tethers reach every visible unique target behind editors",
        "[cycle-v2][canvas][guide-dock][relationship][occlusion]") {
    NodeGraph graph;
    Node firstTarget;
    firstTarget.id = "mesh1";
    firstTarget.bounds = { 310.f, 20.f, 70.f, 60.f };
    graph.addNode(std::move(firstTarget));
    Node secondTarget;
    secondTarget.id = "mesh2";
    secondTarget.bounds = { 10.f, 20.f, 70.f, 60.f };
    graph.addNode(std::move(secondTarget));
    GuideCurveResource guide;
    guide.id = "guide1";
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({
            "guide1",
            "mesh1",
            { 0, GuideCurveField::Time }
    }));
    REQUIRE(graph.assignGuideCurve({
            "guide1",
            "mesh1",
            { 0, GuideCurveField::Red }
    }));
    REQUIRE(graph.assignGuideCurve({
            "guide1",
            "mesh2",
            { 0, GuideCurveField::Time }
    }));
    REQUIRE(graph.guideTargetNodeIds("guide1").size() == 2);

    GraphCompileResult compileResult;
    GraphPreviewResult previewResult;
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 400.f, 200.f });
    viewport.setTransform({}, 1.f);
    NodePalette palette;
    GuideCurveShelfState guideState;
    guideState.hoveredGuideId = "guide1";
    SignalProbeRailState dockState;
    dockState.expanded = true;
    dockState.expandedHeight = 100.f;
    const Rectangle<float> editorOcclusion { 100.f, 90.f, 200.f, 100.f };
    NodeCanvasPresentationFrame frame {
            graph,
            compileResult,
            previewResult,
            viewport,
            palette,
            { 0.f, 0.f, 400.f, 200.f },
            editorOcclusion,
            {},
            {},
            {},
            {},
            std::nullopt,
            {},
            0,
            0,
            -1,
            -1,
            true,
            false,
            { 0.f, 0.f, 400.f, 300.f },
            guideState,
            0.5f,
            dockState,
            {},
            {},
            {}
    };

    Image image(Image::ARGB, 400, 300, true);
    Graphics graphics(image);
    GuideRelationshipPresentation::paintTether(graphics, frame);

    int visiblePixels = 0;
    int occludedPixels = 0;
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            if (image.getPixelAt(x, y).getAlpha() == 0) {
                continue;
            }
            if (editorOcclusion.contains((float) x, (float) y)) {
                ++occludedPixels;
            } else {
                ++visiblePixels;
            }
        }
    }
    REQUIRE(visiblePixels > 0);
    REQUIRE(occludedPixels == 0);

    const auto alphaCount = [&image](Rectangle<int> bounds) {
        int count = 0;
        for (int y = bounds.getY(); y < bounds.getBottom(); ++y) {
            for (int x = bounds.getX(); x < bounds.getRight(); ++x) {
                if (image.getPixelAt(x, y).getAlpha() > 0) {
                    ++count;
                }
            }
        }
        return count;
    };
    REQUIRE(alphaCount({ 339, 74, 12, 12 }) > 0);
    REQUIRE(alphaCount({ 39, 74, 12, 12 }) > 0);
    const auto dock = WorkspaceDock::layout(
            frame.workspaceBounds,
            {
                    dockState.expanded,
                    guideState.minimized,
                    dockState.minimized,
                    dockState.expandedHeight,
                    frame.dockSplitRatio
            });
    const auto guideTile = GuideCurveShelf::tileBoundsFor(
            frame.workspaceBounds,
            dockState,
            frame.dockSplitRatio,
            guideState,
            0);
    WorkspaceDock::paintChrome(graphics, dock, "Curve Guides", "Spies", true, false);
    GuideRelationshipPresentation::paintTetherTerminal(graphics, frame);
    const Point<int> terminal {
            roundToInt(guideTile.getCentreX()),
            roundToInt(dock.dock.getY())
    };
    REQUIRE(alphaCount(Rectangle<int>(12, 12).withCentre(terminal)) > 0);

    frame.canvasOcclusion = { 0.f, 10.f, 400.f, 80.f };
    image.clear(image.getBounds(), Colours::transparentBlack);
    GuideRelationshipPresentation::paintTether(graphics, frame);
    REQUIRE(imageChecksum(image) == imageChecksum(
            Image(Image::ARGB, image.getWidth(), image.getHeight(), true)));
}

TEST_CASE("Signal probe detail uses the audition-note period resolution",
        "[cycle-v2][canvas][probe][detail]") {
    REQUIRE(SignalProbeDetailView::resolutionForMidiNote(48) == 512);
    REQUIRE(SignalProbeDetailView::resolutionForMidiNote(60) == 256);
    REQUIRE(SignalProbeDetailView::resolutionForMidiNote(72) == 128);

    const Rectangle<float> content { 0.f, 0.f, 1200.f, 610.f };
    const Rectangle<float> detail = SignalProbeDetailView::boundsFor(content);
    REQUIRE(content.contains(detail));
    REQUIRE(detail.getWidth() > 700.f);
    REQUIRE(detail.getHeight() > 400.f);
    REQUIRE(detail.contains(SignalProbeDetailView::closeBounds(detail)));
}

TEST_CASE("Signal probe detail resolves the attached Voice Context key value",
        "[cycle-v2][canvas][probe][detail]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node triple = factory.createNode(NodeKind::ModulationTriple, "triple", {});
    for (auto& parameter : triple.parameters) {
        if (parameter.id == "redConstant") {
            parameter.value = String(Arithmetic::getUnitValueForGraphicNote(
                    72,
                    {
                            Constants::LowestMidiNote,
                            Constants::HighestMidiNote
                    }), 9);
        }
    }
    graph.addNode(std::move(triple));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addEdge({
            "triple", "modulation", "voice", "modulation",
            PortDomain::VoiceControlSignal, ConnectionKind::ConfigurationAttachment,
            AttachmentType::ModulationTriple
    });
    graph.addEdge({
            "voice", "context", "mesh", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    });
    graph.addEdge({
            "mesh", "out", "fft", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 2, 0.5f).succeeded());

    REQUIRE(GraphPresentationModel::auditionMidiNoteForProbe(
            graph,
            graph.getSignalProbes().front().id) == 72);

    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    REQUIRE(presentation.previewResult().probes.front().connected);
    REQUIRE(presentation.previewResult().probes.front().frequencyMidiNote == 72);

    const String c3Key(Arithmetic::getUnitValueForGraphicNote(
            48,
            {
                    Constants::LowestMidiNote,
                    Constants::HighestMidiNote
            }), 9);
    const GraphEditResult pitchEdit = GraphEditor().setNodeParameter(
            graph,
            "triple",
            "redConstant",
            "Red",
            c3Key);
    REQUIRE(pitchEdit.succeeded());
    REQUIRE(presentation.refresh(graph, 2, pitchEdit.changes));
    REQUIRE(presentation.previewResult().probes.front().frequencyMidiNote == 48);
}

TEST_CASE("Signal probe preview pitch defaults to C3 without a Modulation Triple",
        "[cycle-v2][canvas][probe][detail][spectral]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "voice", "context", "wave", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    });
    graph.addEdge({
            "wave", "out", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 1, 0.5f).succeeded());

    REQUIRE(GraphPresentationModel::auditionMidiNoteForProbe(
            graph,
            graph.getSignalProbes().front().id) == 48);
}

TEST_CASE("Preview pitch context follows the Modulation Triple key-scale axis",
        "[cycle-v2][canvas][preview][key-scale]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node triple = factory.createNode(NodeKind::ModulationTriple, "triple", {});
    graph.addNode(std::move(triple));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addEdge({
            "triple", "modulation", "voice", "modulation",
            PortDomain::VoiceControlSignal, ConnectionKind::ConfigurationAttachment,
            AttachmentType::ModulationTriple
    });
    graph.addEdge({
            "voice", "context", "mesh", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    });

    const int existingPreviewNote = Arithmetic::getGraphicNoteForValue(
            0.5f,
            {
                    Constants::LowestMidiNote,
                    Constants::HighestMidiNote
            });
    PreviewPitchContext context = PreviewPitchResolver::contextForNode(graph, "mesh");
    REQUIRE(context.midiNote == existingPreviewNote);
    REQUIRE(context.keyScaleAxis == "red");

    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "triple",
            "redSource",
            "Red Source",
            "modWheel").succeeded());
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "triple",
            "yellowSource",
            "Yellow Source",
            "keyScale").succeeded());

    context = PreviewPitchResolver::contextForNode(graph, "mesh");
    REQUIRE(context.midiNote == existingPreviewNote);
    REQUIRE(context.keyScaleAxis == "yellow");
}

TEST_CASE("Signal probe detail capture lazily reruns the addressed traversal at full resolution",
        "[cycle-v2][canvas][probe][detail]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 400.f, 0.f }));
    graph.addEdge({
            "voice", "context", "mesh", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    });
    graph.addEdge({
            "mesh", "out", "fft", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 1, 0.5f).succeeded());

    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    REQUIRE(presentation.previewResult().probes.size() == 1);
    const GraphPreviewResult::SignalProbePreview compactBefore =
            presentation.previewResult().probes.front();
    const size_t resolution = SignalProbeDetailView::resolutionForMidiNote(60);
    const auto detail = presentation.captureProbePreview(
            graph,
            graph.getSignalProbes().front().id,
            resolution,
            60);

    REQUIRE(detail.has_value());
    REQUIRE(detail->connected);
    REQUIRE(detail->gridColumns == resolution / 2);
    REQUIRE(detail->gridRows == resolution);
    REQUIRE(detail->values.size() == detail->gridColumns * resolution);
    const GraphPreviewResult::SignalProbePreview& compactAfter =
            presentation.previewResult().probes.front();
    REQUIRE(compactAfter.gridColumns == compactBefore.gridColumns);
    REQUIRE(compactAfter.gridRows == compactBefore.gridRows);
    REQUIRE(compactAfter.values == compactBefore.values);
}

TEST_CASE("Signal probes inherit spectral mesh render semantics",
        "[cycle-v2][canvas][probe][spectral]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = { { "domain", "Start Domain", "spectral" } };
    Node layer = factory.createNode(NodeKind::SpectralLayer, "layer", {});
    layer.parameters = {
            { "pan", "Pan", "0.5" },
            { "range", "Range", "0.5" },
            { "mode", "Magnitude Mode", "multiplicative" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(std::move(layer));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({
            "voice", "context", "mesh", "context",
            PortDomain::DomainContext, ConnectionKind::Signal
    });
    graph.addEdge({
            "mesh", "out", "layer", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "layer", "out", "ifft", "mag",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 1, 0.5f).succeeded());

    const NodeRenderSemantic semantic = SignalProbeRail::renderSemanticForProbe(
            graph,
            graph.getSignalProbes().front().id);
    REQUIRE(semantic.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(semantic.scalePolicy == RenderScalePolicy::Bipolar);
    REQUIRE(semantic.role == RenderSemanticRole::SpectralMagnitudeMultiplicative);
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
    const auto mappedWithDc = profile.mapSpectrum2DGridToDisplay(
            withDc,
            columns,
            rows);
    const auto mappedWithoutDc = profile.mapSpectrum2DGridToDisplay(
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
    ScopedJuceInitialiser_GUI juce;
    MessageManagerLock messageLock;

    for (const auto& definition : NodeDefinitionRegistry::instance().definitions()) {
        INFO("Missing or invalid icon for node type " << definition.typeId);
        REQUIRE(NodePaletteEntryIconRenderer::hasIcon(definition.kind));
    }
}

TEST_CASE("Guide controls render the shared semantic icon at production size",
        "[cycle-v2][canvas][icons][guide]") {
    ScopedJuceInitialiser_GUI juce;
    MessageManagerLock messageLock;
    Image rendered(Image::ARGB, 30, 17, true);
    const uint64_t blankChecksum = imageChecksum(rendered);
    Graphics graphics(rendered);

    REQUIRE(NodeIconRenderer::hasIcon("guideCurve"));
    NodeIconRenderer::paint(
            graphics,
            "guideCurve",
            rendered.getBounds().toFloat(),
            0.82f);
    REQUIRE(imageChecksum(rendered) != blankChecksum);
}

TEST_CASE("Effect enable actions share full and embedded header geometry",
        "[cycle-v2][canvas][effects][chrome]") {
    const Rectangle<int> fullEditor { 0, 0, 520, 520 };
    const auto full = fullEditorHeaderLayout(fullEditor, true);
    REQUIRE(full.enabled.getWidth() == 28);
    REQUIRE(full.enabled.getHeight() == 28);
    REQUIRE(full.enabled.getRight() + 8 == full.close.getX());
    REQUIRE(full.enabled.getCentreY() == full.close.getCentreY());
    REQUIRE(full.header.contains(full.enabled));

    const Rectangle<float> embeddedEditor { 0.f, 0.f, 900.f, 430.f };
    const auto embedded = embeddedEditorHeaderLayout(embeddedEditor, true);
    REQUIRE(embedded.enabled.getWidth() == Catch::Approx(28.f));
    REQUIRE(embedded.enabled.getHeight() == Catch::Approx(28.f));
    REQUIRE(embedded.enabled.getRight() + 8.f == Catch::Approx(embedded.close.getX()));
    REQUIRE(embedded.enabled.getCentreY() == Catch::Approx(embedded.close.getCentreY()));
    REQUIRE(embedded.header.contains(embedded.enabled));
}

TEST_CASE("Effect enable button has distinct native-size on and bypass states",
        "[cycle-v2][canvas][effects][chrome][icons]") {
    ScopedJuceInitialiser_GUI juce;
    MessageManagerLock messageLock;
    EffectEnableButton button;
    button.setBounds(0, 0, 28, 28);

    const auto render = [&button](bool enabled) {
        button.setToggleState(enabled, dontSendNotification);
        Image image(Image::ARGB, 28, 28, true);
        Graphics graphics(image);
        button.paintButton(graphics, false, false);
        return imageChecksum(image);
    };

    const uint64_t bypassed = render(false);
    const uint64_t enabled = render(true);
    REQUIRE(bypassed != imageChecksum(Image(Image::ARGB, 28, 28, true)));
    REQUIRE(enabled != bypassed);
    REQUIRE(button.getTooltip() == "Enable or bypass effect");
    REQUIRE(button.getWantsKeyboardFocus());

    button.setToggleState(false, dontSendNotification);
    REQUIRE(static_cast<Component&>(button).keyPressed(
            KeyPress(KeyPress::returnKey)));
    MessageManager::getInstance()->runDispatchLoopUntil(50);
    REQUIRE(button.getToggleState());
}

TEST_CASE("Every Envelope purpose has a parseable compact icon",
        "[cycle-v2][canvas][envelope][icons]") {
    ScopedJuceInitialiser_GUI juce;
    MessageManagerLock messageLock;

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
    REQUIRE(bounds.getWidth() == Catch::Approx(1080.f));
    REQUIRE(bounds.getHeight() == Catch::Approx(430.f));

    const auto clampedIrBounds = registry.moduleFor(NodeKind::ImpulseResponse)
            .expandedEditorBounds({ 0.f, 0.f, 900.f, 600.f }, 18.f);
    REQUIRE(clampedIrBounds.getWidth() == Catch::Approx(864.f));
    REQUIRE(clampedIrBounds.getHeight() == Catch::Approx(430.f));

    const auto waveshaperBounds = registry.moduleFor(NodeKind::Waveshaper)
            .expandedEditorBounds({ 0.f, 0.f, 1400.f, 800.f }, 18.f);
    REQUIRE(waveshaperBounds.getWidth() == Catch::Approx(766.f));
    REQUIRE(waveshaperBounds.getHeight() == Catch::Approx(464.f));

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

TEST_CASE("Cable renderer uses one solid grammar with edit-state semantics",
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

    const std::array<NodeCableStyle, 4> styles {
            NodeCableStyle { Colour(0xff42d3cf), false, false, false, false },
            NodeCableStyle { Colour(0xffff5a5f), true, false, false, false },
            NodeCableStyle { Colour(0xff42d3cf), false, true, false, false },
            NodeCableStyle { Colour(0xff42d3cf), false, false, true, false }
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

TEST_CASE("Canvas legend collapses non-signal domains into Control",
        "[cycle-v2][canvas][legend]") {
    REQUIRE(CanvasChromeMetrics::legendFontSize
            == Catch::Approx(CanvasChromeMetrics::microFontSize * 1.3f));
    REQUIRE(CanvasChromeMetrics::legendLineLength == Catch::Approx(17.f * 1.3f));
    REQUIRE(CanvasChromeMetrics::legendLineWidth == Catch::Approx(2.f * 1.3f));
    REQUIRE(CanvasChromeMetrics::legendRowStride == Catch::Approx(20.f * 1.3f));

    const Colour control = colourForDomain(PortDomain::ControlSignal);
    REQUIRE(colourForDomain(PortDomain::DomainContext) == control);
    REQUIRE(colourForDomain(PortDomain::MeshField) == control);
    REQUIRE(colourForDomain(PortDomain::EnvelopeSignal) == control);
    REQUIRE(colourForDomain(PortDomain::PitchSignal) == control);
    REQUIRE(colourForDomain(PortDomain::VoiceControlSignal) == control);
    REQUIRE(colourForDomain(PortDomain::TimeSignal) != control);
    REQUIRE(colourForDomain(PortDomain::SpectralMagnitudeSignal) != control);
    REQUIRE(colourForDomain(PortDomain::SpectralPhaseSignal) != control);
}

TEST_CASE("Voice context compact presentation retains its selector and summary",
        "[cycle-v2][canvas][compact-editor]") {
    Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});

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
