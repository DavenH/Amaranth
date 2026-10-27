#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "Nodes/Guide/GuideGraphEditor.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeParameterMap.h"
#include "Graph/GraphSerializer.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "EnvelopeAuthoringTestSupport.h"
#include "TrimeshAuthoringTestSupport.h"
#include "Nodes/Curve/Editor/CurveNodeEditorFactory.h"
#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "Nodes/Envelope/Editor/EnvelopeAxisScaleSelector.h"
#include "Nodes/Envelope/Editor/EnvelopeEditorComponent.h"
#include "Nodes/Envelope/Rendering/EnvelopeMorphSurfacePreview.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"
#include <UI/Panels/ZoomPanel.h>
#include <UI/Panels/PanelInputHostComponent.h>
#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"
#include "Nodes/ImpulseResponse/Editor/ImpulseResponseEditorComponent.h"
#include "Nodes/ImpulseResponse/ImpulseResponseAnalysis.h"
#include "Nodes/Trimesh/Editor/TrimeshExpandedEditorComponent.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "Nodes/Trimesh/Dsp/TrimeshGuidePreparation.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Unison/UnisonNode.h"
#include "Nodes/Unison/UnisonPreviewPainter.h"
#include "Nodes/Waveshaper/Editor/WaveshaperEditorComponent.h"
#include "Nodes/Waveshaper/WaveshaperSignalProcessor.h"
#include "Runtime/PresentationGestureSession.h"
#include "UI/EffectEnableButton.h"
#include "UI/EditorChromeLayout.h"
#include "UI/Editors/NodePropertyControlBinding.h"
#include "UI/Editors/MeshEditorLayout.h"
#include "UI/Editors/MorphSpaceLayout.h"
#include "Nodes/Envelope/Editor/EnvelopeMorphControls.h"
#include "UI/Editors/ProcessingScopeSelector.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertySegmentedSelector.h"
#include "UI/EnvelopePurposeSelector.h"
#include "UI/NodeCanvasAutomationController.h"
#include "UI/NodeCanvasAutomationInspector.h"
#include "UI/NodeEditorHost.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodePreviewResources.h"

#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>
#include <Audio/CycleDsp/IrModel.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>

using namespace CycleV2;
using namespace juce;

namespace {

class CurveTableScope {
public:
    CurveTableScope() { Curve::calcTable(); }
    ~CurveTableScope() { Curve::deleteTable(); }
};

MouseEvent curvePanelMouseEvent(
        Component& component,
        Point<float> position,
        ModifierKeys modifiers,
        Point<float> mouseDownPosition,
        bool dragged,
        int clickCount = 1) {
    const Time now = Time::getCurrentTime();
    return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            modifiers,
            1.f,
            0.f,
            0.f,
            0.f,
            0.f,
            &component,
            &component,
            now,
            mouseDownPosition,
            now,
            clickCount,
            dragged
    };
}

void addUnrelatedInteractionState(NodeGraph& graph) {
    GraphNodeFactory factory;
    for (int index = 0; index < 128; ++index) {
        graph.addNode(factory.createNode(
                NodeKind::Add,
                "unrelated" + String(index),
                { (float) index, (float) index }));
    }
    AudioSampleResource audio { "unrelated-audio", "Unrelated.wav", 48000.0, {} };
    audio.samples.resize(16384);
    graph.addAudioResource(std::move(audio));
}

TEST_CASE("Curve snapshot capture requires full framebuffer residency",
        "[cycle-v2][node-editor-host][performance]") {
    const Rectangle<int> viewport { 0, 0, 800, 600 };
    const auto visible = curvePanelFramebufferReadBounds(
            { 100.f, 80.f, 220.f, 140.f }, 1.f, viewport);
    REQUIRE(visible.has_value());
    REQUIRE(*visible == Rectangle<int>(100, 380, 220, 140));

    REQUIRE_FALSE(curvePanelFramebufferReadBounds(
            { -1.f, 80.f, 220.f, 140.f }, 1.f, viewport).has_value());
    REQUIRE_FALSE(curvePanelFramebufferReadBounds(
            { 700.f, 80.f, 220.f, 140.f }, 1.f, viewport).has_value());
    REQUIRE_FALSE(curvePanelFramebufferReadBounds(
            { 100.f, -1.f, 220.f, 140.f }, 1.f, viewport).has_value());
    REQUIRE_FALSE(curvePanelFramebufferReadBounds(
            { 100.f, 540.f, 220.f, 140.f }, 1.f, viewport).has_value());

    const Rectangle<int> retinaViewport { 10, 20, 1600, 1200 };
    const auto retina = curvePanelFramebufferReadBounds(
            { 100.f, 80.f, 220.f, 140.f }, 2.f, retinaViewport);
    REQUIRE(retina.has_value());
    REQUIRE(*retina == Rectangle<int>(210, 780, 440, 280));
}

TEST_CASE("Curve preview snapshots are reused until a rendering dependency changes",
        "[cycle-v2][node-editor-host][performance]") {
    CurvePanelPreviewRenderCache cache;
    const CurvePanelPreviewRenderCache::Key initial {
            180.f, 96.f, 2.f, 7, 11, 13, 17, { 0.1f, 0.2f, 0.7f, 0.6f }
    };

    REQUIRE_FALSE(cache.canReuse(initial));
    cache.didRender(initial);
    REQUIRE(cache.canReuse(initial));

    auto changed = initial;
    changed.width += 1.f;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.height += 1.f;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.scaleFactor = 1.f;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    ++changed.modelRevision;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    ++changed.contentRevision;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    ++changed.presentationRevision;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    ++changed.invalidationGeneration;
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.view.translate(0.1f, 0.f);
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.view.translate(0.f, 0.1f);
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.view.setWidth(0.5f);
    REQUIRE_FALSE(cache.canReuse(changed));
    changed = initial;
    changed.view.setHeight(0.4f);
    REQUIRE_FALSE(cache.canReuse(changed));

    cache.invalidate();
    REQUIRE_FALSE(cache.canReuse(initial));
    const auto diagnostics = cache.diagnostics();
    REQUIRE(diagnostics.hits == 1);
    REQUIRE(diagnostics.misses == 13);
}

TEST_CASE("Curve framebuffer snapshots preserve channels while flipping rows and normalizing alpha",
        "[cycle-v2][node-editor-host][curve-snapshot][regression]") {
    std::array<PixelARGB, 4> pixels;
    pixels[0].setARGB(0, 17, 36, 91);
    pixels[1].setARGB(64, 193, 28, 47);
    pixels[2].setARGB(128, 61, 174, 32);
    pixels[3].setARGB(255, 43, 72, 219);
    bool visible {};
    const auto image = curvePanelSnapshotFromPixels(pixels.data(), 2, 2, visible);
    REQUIRE(visible);
    REQUIRE(image.getPixelAt(0, 0) == Colour::fromRGB(61, 174, 32));
    REQUIRE(image.getPixelAt(1, 0) == Colour::fromRGB(43, 72, 219));
    REQUIRE(image.getPixelAt(0, 1) == Colour::fromRGB(17, 36, 91));
    REQUIRE(image.getPixelAt(1, 1) == Colour::fromRGB(193, 28, 47));

    PixelARGB dark;
    dark.setARGB(0, 80, 40, 40);
    REQUIRE(curvePanelSnapshotFromPixels(&dark, 1, 1, visible).isValid());
    REQUIRE_FALSE(visible);
    dark.setARGB(0, 81, 40, 40);
    REQUIRE(curvePanelSnapshotFromPixels(&dark, 1, 1, visible).isValid());
    REQUIRE(visible);
    REQUIRE_FALSE(curvePanelSnapshotFromPixels(nullptr, 1, 1, visible).isValid());
    REQUIRE_FALSE(visible);
}

TEST_CASE("Curve document replacement clears snapshots and changes preview identity",
        "[cycle-v2][node-editor-host][regression]") {
    CurvePanelSnapshotCache snapshot;
    const uint64_t emptyRevision = snapshot.revision();
    Image rendered(Image::ARGB, 8, 8, true);
    Graphics renderedGraphics(rendered);
    renderedGraphics.fillAll(Colours::white);
    REQUIRE(snapshot.publish(rendered, true));
    REQUIRE(snapshot.revision() > emptyRevision);
    const uint64_t publishedRevision = snapshot.revision();

    Image destination(Image::ARGB, 8, 8, true);
    Graphics destinationGraphics(destination);
    REQUIRE(snapshot.paint(destinationGraphics, destination.getBounds().toFloat(), false));
    snapshot.clear();
    REQUIRE_FALSE(snapshot.paint(
            destinationGraphics,
            destination.getBounds().toFloat(),
            false));
    REQUIRE(snapshot.revision() > publishedRevision);

    const uint64_t clearedRevision = snapshot.revision();
    Image emptyCapture(Image::RGB, 8, 8, true);
    REQUIRE_FALSE(snapshot.publish(emptyCapture, false));
    REQUIRE(snapshot.revision() == clearedRevision);
    REQUIRE_FALSE(snapshot.paint(
            destinationGraphics,
            destination.getBounds().toFloat(),
            false));

    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(NodeKind::Waveshaper);
    const uint64_t originalPreviewRevision = widget.previewRevision();
    widget.resetDocumentPresentation();
    REQUIRE(widget.previewRevision() > originalPreviewRevision);
}

struct EditorStats {
    int creations {};
    int destructions {};
    int bindings {};
    String boundNodeId;
};

class MockEditor final : public NodeEditor {
public:
    explicit MockEditor(EditorStats& statsToUse) : stats(statsToUse) {
        ++stats.creations;
        control.setComponentID("mock.control");
        control.setBounds(5, 6, 20, 10);
        editorComponent.addAndMakeVisible(control);
    }
    ~MockEditor() override { ++stats.destructions; }

    Component& component() override { return editorComponent; }
    void bind(const Node& node) override {
        ++stats.bindings;
        stats.boundNodeId = node.id;
    }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("mock", true);
    }
    Rectangle<float> panelBoundsForAutomation() const override { return { 1.f, 2.f, 3.f, 4.f }; }
    void releaseOpenGLResources() override {}

private:
    EditorStats& stats;
    Component editorComponent;
    TextButton control;
};

class MockFactory final : public NodeEditorFactory {
public:
    explicit MockFactory(EditorStats& statsToUse) : stats(statsToUse) {}

    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext&) const override {
        return std::make_unique<MockEditor>(stats);
    }

private:
    EditorStats& stats;
};

class MockFactories final : public NodeEditorFactoryProvider {
public:
    explicit MockFactories(EditorStats& stats) : factory(stats) {}

    const NodeEditorFactory* find(NodeKind kind) const override {
        return kind == NodeKind::Waveshaper ? &factory : nullptr;
    }

private:
    MockFactory factory;
};

class NullCommands : public NodeEditorCommands {
public:
    bool publishCurveState(const String&, NodeModelStatePtr,
            const std::vector<NodeParameter>&) override { return true; }
    void beginCurveTransaction() override {}
    void commitCurveTransaction() override {}
    bool setTrimeshPrimaryAxisValue(const String&, const String&) override { return true; }
    bool toggleTrimeshLinkAxisValue(const String&, const String&) override { return true; }
    bool beginTrimeshMorphEdit(const String&, const String&, float) override { return true; }
    bool updateTrimeshMorphEditValue(float) override { return true; }
    void endTrimeshMorphEdit() override {}
    bool beginTrimeshVertexParameterEdit(const String&, const String&, float) override { return true; }
    bool updateTrimeshVertexParameterEditValue(float) override { return true; }
    void endTrimeshVertexParameterEdit() override {}
    bool showTrimeshGuideAttachmentMenu(
            const String&, const String&, Rectangle<int>) override { return true; }
    bool selectTrimeshVertexIndex(const String&, int) override { return true; }
    void persistTrimeshMeshEdits(const String&, bool) override {}
};

class RecordingParameterCommands final : public NullCommands {
public:
    bool beginNodeParameterEdit(
            const String&,
            const String&,
            const String&,
            float value) override {
        ++begins;
        values.push_back(value);
        return true;
    }

    bool updateNodeParameterEditValue(float value) override {
        ++updates;
        values.push_back(value);
        return true;
    }

    void endNodeParameterEdit() override { ++ends; }

    int begins {};
    int updates {};
    int ends {};
    std::vector<float> values;
};

class NullPresentation final : public NodeEditorPresentation {
public:
    void closeNodeEditor() override {}
    void repaintNodeEditor(bool) override {}
    void selectEditedNode(const String&) override {}
    void setNodeEditorStatus(const String&) override {}
    bool beginNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document,
            bool downstreamFeedback = true) override {
        return session.beginGraphGesture(
                "editor:" + nodeId,
                commands,
                document,
                ProbeRefreshMode::OnGestureCommit,
                0,
                true,
                downstreamFeedback);
    }
    void finishNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document,
            const String& = {}) override {
        session.finishGraphGesture("editor:" + nodeId, commands, document);
    }
    void cancelNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands) override {
        session.cancelGraphGesture("editor:" + nodeId, commands);
    }
    void scheduleNodeEditorRefresh() override {}
    void flushNodeEditorRefresh() override {}
    void refreshNodeEditorPresentation() override {}
    Point<float> nodeEditorCreationPosition() const override { return {}; }
    void rebindNodeEditor() override {}

private:
    PresentationGestureSession session;
};

class RecordingPresentation final : public NodeEditorPresentation {
public:
    void closeNodeEditor() override {}
    void repaintNodeEditor(bool) override { ++repaints; }
    void selectEditedNode(const String&) override {}
    void setNodeEditorStatus(const String&) override {}
    bool beginNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document,
            bool downstreamFeedback = true) override {
        gestureDownstreamFeedback.push_back(downstreamFeedback);
        return session.beginGraphGesture(
                "editor:" + nodeId, commands, document, refreshMode, 0, true,
                downstreamFeedback);
    }
    void finishNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands,
            const GraphDocument& document,
            const String& localField = {}) override {
        const auto finished = session.finishGraphGesture(
                "editor:" + nodeId, commands, document);
        if (finished.changed && finished.durableChanged) {
            ++gestureCommits;
            if (localField.isNotEmpty()) {
                ++localCommits;
            }
        }
    }
    void cancelNodeEditorGesture(
            const String& nodeId,
            GraphCommandDispatcher& commands) override {
        session.cancelGraphGesture("editor:" + nodeId, commands);
    }
    void scheduleNodeEditorRefresh() override { ++scheduledRefreshes; }
    void flushNodeEditorRefresh() override {}
    void refreshNodeEditorPresentation() override { ++immediateRefreshes; }
    Point<float> nodeEditorCreationPosition() const override { return {}; }
    void rebindNodeEditor() override { ++rebinds; }
    void rebindNodeEditorTransient() override { ++transientRebinds; }
    void recordNodeEditorMovement(
            const String& nodeId,
            const String&,
            uint64_t fingerprint,
            std::optional<UpdateProduct> localProduct) override {
        session.recordGraphMovement("editor:" + nodeId, fingerprint);
        movementLocalProducts.push_back(localProduct);
        ++recordedMovements;
    }
    void commitNodeEditorLocalState(
            const String&,
            const String&,
            uint64_t,
            uint64_t) override {
        ++localCommits;
    }
    ProbeRefreshMode probeRefreshMode() const override { return refreshMode; }

    ProbeRefreshMode refreshMode { ProbeRefreshMode::OnGestureCommit };
    int repaints {};
    int scheduledRefreshes {};
    int immediateRefreshes {};
    int localCommits {};
    int rebinds {};
    int transientRebinds {};
    int recordedMovements {};
    int gestureCommits {};
    std::vector<bool> gestureDownstreamFeedback;
    std::vector<std::optional<UpdateProduct>> movementLocalProducts;

private:
    PresentationGestureSession session;
};

class NullResources : public NodeEditorResources {
public:
    CurveEditorWidget* curveEditorWidget(const Node&) override { return nullptr; }
    TrimeshWidget* trimeshWidget(const Node& node) override {
        ++synchronizingTrimeshLookups;
        if (activeTrimesh != nullptr) {
            activeTrimesh->syncFromNode(node);
        }
        return activeTrimesh;
    }
    TrimeshWidget* findTrimeshWidget(const String&) override { return activeTrimesh; }
    TrimeshRenderProfile trimeshRenderProfile(const Node&) const override {
        return TrimeshRenderProfile::fromDomain(trimeshDomain);
    }
    std::array<String, 6> trimeshGuideLabels(const Node&) override { return {}; }
    void paintNodePreview(Graphics&, const Node&, Rectangle<float>) override {}
    UnisonPreviewContext unisonPreviewContext() const override {
        UnisonPreviewContext result;
        result.voiceDurationSeconds = voiceLengthSeconds;
        return result;
    }
    TrimeshWidget* activeTrimesh {};
    PortDomain trimeshDomain { PortDomain::TimeSignal };
    int synchronizingTrimeshLookups {};
    double voiceLengthSeconds { 1.0 };
};

class EnvelopeEditorResources final : public NullResources {
public:
    CurveEditorWidget* curveEditorWidget(const Node&) override { return &widget; }

private:
    CurveEditorWidget widget { NodeKind::Envelope };
};

class PreviewTrimeshEditorResources final : public NullResources {
public:
    TrimeshWidget* trimeshWidget(const Node& node) override {
        return &previews->trimeshWidget(node, graph);
    }
    TrimeshWidget* findTrimeshWidget(const String& nodeId) override {
        return previews->findTrimeshWidget(nodeId);
    }

    NodePreviewResources* previews {};
    const NodeGraph* graph {};
};

class PreviewEnvelopeEditorResources final : public NullResources {
public:
    CurveEditorWidget* curveEditorWidget(const Node& node) override {
        return &previews->curveEditorWidget(node);
    }
    void syncCurveGuideContext(CurveEditorWidget&, const Node& node) override {
        previews->syncCurveEditorWidget(node);
    }

    NodePreviewResources* previews {};
};

std::array<float, 3> trimeshEditorMorphPosition(const NodeEditorHost& host) {
    DynamicObject state;
    host.appendAutomationState(state);
    const auto* sliders = state.getProperty("morphSliders").getArray();
    REQUIRE(sliders != nullptr);
    std::array<float, 3> result {};
    const char* ids[] { "yellow", "red", "blue" };
    for (int axis = 0; axis < 3; ++axis) {
        const auto slider = std::find_if(sliders->begin(), sliders->end(), [&](const var& value) {
            return value.getProperty("id", {}).toString() == ids[axis];
        });
        REQUIRE(slider != sliders->end());
        result[(size_t) axis] = (float) slider->getProperty("value", {});
    }
    return result;
}

class RecordingVoiceCommands final : public NullCommands {
public:
    bool beginNodeParameterEdit(
            const String&,
            const String& parameterId,
            const String&,
            float value) override {
        activeParameterId = parameterId;
        numericValues.push_back(value);
        ++begins;
        return true;
    }

    bool updateNodeParameterEditValue(float value) override {
        numericValues.push_back(value);
        ++updates;
        return true;
    }

    void endNodeParameterEdit() override {
        ++ends;
        activeParameterId = {};
    }

    bool setNodeParameterValue(
            const String&,
            const String& parameterId,
            const String&,
            float value) override {
        immediateParameterId = parameterId;
        immediateValue = value;
        return true;
    }

    bool setNodeParameterText(
            const String&,
            const String& parameterId,
            const String&,
            const String& value) override {
        textParameterId = parameterId;
        textValue = value;
        return true;
    }

    String activeParameterId;
    String immediateParameterId;
    String textParameterId;
    String textValue;
    float immediateValue {};
    int begins {};
    int updates {};
    int ends {};
    std::vector<float> numericValues;
};

TEST_CASE("Trimesh compact preview ignores a divergent captured heatmap",
        "[cycle-v2][canvas][preview][trimesh][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);

    NodePreviewResult divergentRuntime;
    divergentRuntime.nodeId = "mesh";
    divergentRuntime.role = PreviewModuleRole::MeshSurface;
    divergentRuntime.primary = { 0.f, 1.f, 1.f, 0.f };
    divergentRuntime.gridColumns = 2;
    divergentRuntime.gridRows = 2;
    divergentRuntime.domain = PortDomain::SpectralPhaseSignal;
    const Node& node = *document.graph().findNode("mesh");
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralPhaseSignal);
    const Rectangle<float> bounds { 0.f, 0.f, 120.f, 96.f };

    const auto render = [&](const NodePreviewResult* runtime) {
        NodePreviewResources resources(editorCommands);
        resources.setDurableGraph(&document.graph());
        NodePreviewRenderer renderer(resources);
        Image image(Image::ARGB, 120, 96, true);
        Graphics graphics(image);
        renderer.paint(graphics, {
                node,
                runtime,
                bounds,
                profile,
                1.f,
                true
        });
        return image;
    };

    const Image authoritative = render(nullptr);
    const Image withRuntime = render(&divergentRuntime);
    REQUIRE(authoritative.isValid());
    REQUIRE(withRuntime.isValid());
    const auto checksum = [](const Image& image) {
        uint64_t result = 1469598103934665603ULL;
        for (int y = 0; y < image.getHeight(); ++y) {
            for (int x = 0; x < image.getWidth(); ++x) {
                result ^= image.getPixelAt(x, y).getARGB();
                result *= 1099511628211ULL;
            }
        }
        return result;
    };
    REQUIRE(checksum(withRuntime) == checksum(authoritative));
}

TEST_CASE("Trimesh preview retains the durable graph after a transient refresh",
        "[cycle-v2][canvas][preview][trimesh][lifecycle][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);
    NodePreviewResources resources(editorCommands);
    resources.setDurableGraph(&document.graph());

    {
        NodeGraph transient = NodeGraph::createEditingOverlay(document.graph());
        GraphChangeSet changes;
        changes.nodeIds.push_back("mesh");
        resources.refreshGraph(transient, changes);
        resources.trimeshWidget(*transient.findNode("mesh"), &transient);
    }

    const Node& node = *document.graph().findNode("mesh");
    TrimeshWidget& widget = resources.trimeshWidget(node, &document.graph());
    REQUIRE(widget.guideContextKey().isNotEmpty());
}

TEST_CASE("Transient preview refresh retains the durable graph for Trimesh resources",
        "[cycle-v2][canvas][preview][trimesh][lifetime][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GuideCurveResource guide;
    guide.id = "guide1";
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({
            "guide1",
            "mesh",
            { 0, GuideCurveField::Time }
    }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);
    NodePreviewResources resources(editorCommands);
    resources.setDurableGraph(&document.graph());

    const Node& node = *document.graph().findNode("mesh");
    const String durableKey = resources.trimeshWidget(node).guideContextKey();
    NodeGraph transient = document.graph();
    REQUIRE(GuideGraphEditor().renameGuideCurve(
            transient, "guide1", "Transient guide").succeeded());
    GraphChangeSet transientChange;
    transientChange.guidesChanged = true;
    resources.refreshGraph(transient, transientChange);

    REQUIRE(resources.trimeshWidget(node).guideContextKey() == durableKey);
}

TEST_CASE("Reverb heatmap palette follows its runtime spectral domain",
        "[cycle-v2][canvas][preview][reverb][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);
    NodePreviewResources resources(editorCommands);
    resources.setDurableGraph(&document.graph());
    NodePreviewRenderer renderer(resources);
    const Node& node = *document.graph().findNode("reverb");
    NodePreviewResult runtime {
            "reverb",
            PreviewModuleRole::ReverbSpectrogram,
            { 0.f, 0.25f, 0.75f, 1.f }
    };
    runtime.gridColumns = 2;
    runtime.gridRows = 2;
    runtime.domain = PortDomain::SpectralMagnitudeSignal;

    const auto render = [&](PortDomain requestDomain, const NodePreviewResult* preview) {
        Image image(Image::ARGB, 96, 64, true);
        Graphics graphics(image);
        renderer.paint(graphics, {
                node,
                preview,
                image.getBounds().toFloat(),
                TrimeshRenderProfile::fromDomain(requestDomain),
                1.f,
                true
        });
        return image;
    };

    const Image timeRequest = render(PortDomain::TimeSignal, &runtime);
    const Image magnitudeRequest = render(PortDomain::SpectralMagnitudeSignal, &runtime);
    REQUIRE(timeRequest.isValid());
    REQUIRE(magnitudeRequest.isValid());
    const auto checksum = [](const Image& image) {
        uint64_t result = 1469598103934665603ULL;
        for (int y = 0; y < image.getHeight(); ++y) {
            for (int x = 0; x < image.getWidth(); ++x) {
                result ^= image.getPixelAt(x, y).getARGB();
                result *= 1099511628211ULL;
            }
        }
        return result;
    };
    REQUIRE(checksum(timeRequest) == checksum(magnitudeRequest));
    const Image transientWithoutRuntime = render(
            PortDomain::TimeSignal,
            nullptr);
    REQUIRE(checksum(transientWithoutRuntime) == checksum(timeRequest));
}

TEST_CASE("First compact Envelope paint synchronizes its durable curve model",
        "[cycle-v2][canvas][preview][envelope][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Component canvas;
    NodeGraph graph;
    Node envelope = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    EnvelopeNodeModel envelopeModel;
    REQUIRE(envelopeModel.synchronizeFromMesh(nullptr));
    envelope.model = CurveNodeModelState::copyOf(
            envelopeModel,
            0.5f,
            0.5f,
            envelopeModel.revision() + 7);
    graph.addNode(std::move(envelope));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);
    NodePreviewResources resources(editorCommands);
    resources.setDurableGraph(&document.graph());
    NodePreviewRenderer renderer(resources);
    const Node& node = *document.graph().findNode("env");

    Image image(Image::ARGB, 180, 140, true);
    Graphics graphics(image);
    renderer.paint(graphics, {
            node,
            nullptr,
            image.getBounds().toFloat(),
            TrimeshRenderProfile::fromDomain(PortDomain::ControlSignal),
            1.f,
            true
    });

    REQUIRE(resources.curveEditorWidget(node).modelRevision() == node.model->revision());
}

TEST_CASE("Compact Envelope paint synchronizes component Guide curves",
        "[cycle-v2][canvas][preview][envelope][guides][preset][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Component canvas;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "purpose", "Purpose", "pitch").succeeded());
    REQUIRE(GuideGraphEditor().createGuideCurve(graph).succeeded());
    const GuideCurveResource* createdGuide = graph.findGuideCurve("guide1");
    REQUIRE(createdGuide != nullptr);
    FlatCurveModel guideModel;
    REQUIRE(guideModel.replaceVertices({
            { 1, 0.f, 1.f, 1.f },
            { 2, 1.f, 1.f, 1.f }
    }));
    guideModel.setPublicationRevision(createdGuide->model->revision() + 1);
    REQUIRE(GuideGraphEditor().replaceGuideCurve(
            graph,
            "guide1",
            CurveNodeModelState::copyOf(guideModel, guideModel.revision()),
            {
                    { "enabled", "Enabled", "1" },
                    { "noise", "Noise", "0" },
                    { "dcOffset", "DC Offset", "0" },
                    { "phase", "Phase", "0" }
            }).succeeded());
    REQUIRE(GuideGraphEditor().assignGuideCurveToMeshComponent(
            graph, "guide1", "env", 0, "amp").succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NullPresentation presentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            presentation,
            editorResources);
    NodePreviewResources resources(editorCommands);
    resources.setDurableGraph(&document.graph());
    NodePreviewRenderer renderer(resources);
    Image image(Image::ARGB, 180, 140, true);
    Graphics graphics(image);
    const auto paint = [&] {
        const Node& node = *document.graph().findNode("env");
        renderer.paint(graphics, {
                node,
                nullptr,
                image.getBounds().toFloat(),
                TrimeshRenderProfile::fromDomain(PortDomain::EnvelopeSignal),
                1.f,
                true
        });
        return JSON::toString(
                resources.curveEditorWidget(node).automationState().getProperty(
                        "waveformPoints", {}),
                true);
    };

    const String enabledWaveform = paint();
    const GuideCurveResource* guide = document.graph().findGuideCurve("guide1");
    REQUIRE(guide != nullptr);
    FlatCurveModel editedGuideModel;
    REQUIRE(editedGuideModel.replaceVertices({
            { 1, 0.f, 0.f, 1.f },
            { 2, 1.f, 0.f, 1.f }
    }));
    editedGuideModel.setPublicationRevision(guide->model->revision() + 1);
    graphCommands.beginTransientEdit();
    REQUIRE(graphCommands.publishGuideCurveState({
            "guide1",
            guide->revision,
            CurveNodeModelState::copyOf(
                    editedGuideModel,
                    editedGuideModel.revision()),
            {
                    { "enabled", "Enabled", "1" },
                    { "noise", "Noise", String(guide->noise) },
                    { "dcOffset", "DC Offset", String(guide->dcOffset) },
                    { "phase", "Phase", String(guide->phase) }
            }
    }).succeeded());
    resources.refreshGraph(
            graphCommands.editingGraph(),
            graphCommands.transientChanges());
    resources.clearCachedSprites();
    const String editedWaveform = paint();
    graphCommands.commitTransientEdit();

    REQUIRE(enabledWaveform != editedWaveform);
    const var state = resources.curveEditorWidget(
            *document.graph().findNode("env")).automationState();
    REQUIRE(static_cast<double>(state.getProperty("verticalZoomHeight", {})) < 1.0);
}

std::vector<double> irEditableSamplesAt(
        size_t sampleCount,
        const Array<var>& waveformPoints) {
    std::vector<double> result;
    result.reserve(sampleCount);
    int waveformIndex = 0;
    for (size_t index = 0; index < sampleCount; ++index) {
        const double proportion = static_cast<double>(index)
                / static_cast<double>(sampleCount - 1);
        const double targetX = CycleDsp::irDomainPadding
                + (1.0 - CycleDsp::irDomainPadding) * proportion;
        while (waveformIndex + 1 < waveformPoints.size()) {
            const double currentDistance = std::abs(
                    static_cast<double>(waveformPoints[waveformIndex]
                            .getProperty("x", {})) - targetX);
            const double nextDistance = std::abs(
                    static_cast<double>(waveformPoints[waveformIndex + 1]
                            .getProperty("x", {})) - targetX);
            if (nextDistance >= currentDistance) {
                break;
            }
            ++waveformIndex;
        }
        result.push_back(waveformPoints[waveformIndex].getProperty("y", {}));
    }
    return result;
}

double irSampledCurveIdentityRootMeanSquare(
        const std::vector<float>& samples,
        const std::vector<double>& editableSamples) {
    double squaredError = 0.0;
    for (size_t index = 0; index < samples.size(); ++index) {
        const double sampledValue = samples[index] * 0.5 + 0.5;
        const double error = editableSamples[index] - sampledValue;
        squaredError += error * error;
    }
    return std::sqrt(squaredError / static_cast<double>(samples.size()));
}

template<typename Sample>
int significantTurningPointCount(
        const std::vector<Sample>& samples,
        double minimumDelta) {
    int previousDirection = 0;
    int result = 0;
    for (size_t index = 1; index < samples.size(); ++index) {
        const double delta = static_cast<double>(samples[index])
                - static_cast<double>(samples[index - 1]);
        const int direction = delta > minimumDelta ? 1 : delta < -minimumDelta ? -1 : 0;
        if (direction == 0) {
            continue;
        }
        if (previousDirection != 0 && direction != previousDirection) {
            ++result;
        }
        previousDirection = direction;
    }
    return result;
}

class RecordingCurveDelegate final : public CurveExpandedEditorDelegate {
public:
    void closeCurveEditor() override {}
    void repaintCurveEditorOpenGL() override { events.add("repaint"); }

    bool publishCurveState(
            NodeModelStatePtr model,
            const std::vector<NodeParameter>&) override {
        publications.push_back(std::move(model));
        events.add("publish");
        return true;
    }

    bool setNodeParameterText(
            const String& parameterId,
            const String&,
            const String& value) override {
        textParameterId = parameterId;
        textValue = value;
        events.add("parameter");
        return true;
    }

    void beginCurveTransaction() override { events.add("begin"); }
    void commitCurveTransaction() override { events.add("commit"); }
    bool cancelCurveTransaction() override {
        if (!allowCancel) {
            return false;
        }
        events.add("cancel");
        return true;
    }
    void setCurveEditorStatus(const String& message) override { status = message; }
    bool setAudioResource(NodeAudioResourceEdit edit) override {
        appliedResource = std::move(edit);
        return true;
    }
    bool removeAudioResource() override {
        if (!resourceRemovalSucceeds) {
            return false;
        }
        ++resourceRemovals;
        resource.reset();
        return true;
    }
    std::optional<NodeAudioResourceSummary> audioResourceSummary() const override {
        return resource;
    }
    bool showEnvelopeGuideAttachmentMenu(
            const String& field,
            Rectangle<int> targetScreenArea) override {
        guideMenuField = field;
        guideMenuTarget = targetScreenArea;
        return true;
    }

    StringArray events;
    std::vector<NodeModelStatePtr> publications;
    String status;
    String textParameterId;
    String textValue;
    String guideMenuField;
    Rectangle<int> guideMenuTarget;
    NodeAudioResourceEdit appliedResource;
    std::optional<NodeAudioResourceSummary> resource;
    bool resourceRemovalSucceeds { true };
    bool allowCancel {};
    int resourceRemovals {};
};

class LifecycleCurveEditor final : public CurveExpandedEditorComponent {
public:
    explicit LifecycleCurveEditor(CurveEditorWidget& widget) :
            CurveExpandedEditorComponent(widget)
        ,   slider(*this, "Value")
        ,   toggle(*this, "Enabled") {
        bindContinuousControl(slider);
        bindDiscreteControl(toggle);
        bindDiscreteControl(menu);
        bindDiscreteAction(action, [this] {
            actionPerformed = true;
        });
    }

    Rectangle<float> editorPanelBounds() const override { return {}; }
    Rectangle<float> editorControlBounds() const override { return {}; }
    void paintEditor(Graphics&) override {}
    void layoutEditor() override {}
    void syncEditorFromNode() override { widget.syncFromNode(node); }
    void applyEditorStateToWidget() override {}
    std::vector<NodeParameter> editorControls() const override { return {}; }
    void appendEditorAutomation(DynamicObject&) const override {}

    LabeledParameterSlider slider;
    ParameterToggle toggle;
    ComboBox menu;
    TextButton action;
    bool actionPerformed {};
};

Node node(String id, NodeKind kind) {
    Node result;
    result.id = std::move(id);
    result.kind = kind;
    return result;
}

Rectangle<float> rectangleProperty(const var& state, const Identifier& name) {
    const auto* bounds = state.getProperty(name, {}).getDynamicObject();
    if (bounds == nullptr) {
        return {};
    }
    return {
        static_cast<float>(bounds->getProperty("x")),
        static_cast<float>(bounds->getProperty("y")),
        static_cast<float>(bounds->getProperty("width")),
        static_cast<float>(bounds->getProperty("height"))
    };
}

TEST_CASE("Node canvas automation controller routes aliases and owns diagnostics",
        "[cycle-v2][canvas][automation]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    canvas.setBounds(0, 0, 800, 600);

    NodeGraph graph;
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    GraphPresentationModel graphPresentation;
    REQUIRE(graphPresentation.refresh(document.graph(), document.revision()));

    NullPresentation editorPresentation;
    NullResources editorResources;
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            editorPresentation,
            editorResources);
    NodePreviewResources previewResources(editorCommands);
    EditorStats editorStats;
    MockFactories factories(editorStats);
    NodeEditorHost host(
            canvas,
            editorCommands,
            editorPresentation,
            editorResources,
            factories);
    NodeCanvasAuthoring authoring(
            document,
            graphCommands,
            graphPresentation,
            editorCommands);
    NodeCanvasViewport viewport;
    viewport.setBounds(canvas.getLocalBounds().toFloat());
    NodeCanvasAutomationController automation({
            canvas,
            document,
            graphPresentation,
            viewport,
            authoring,
            host,
            previewResources
        });

    REQUIRE(NodeCanvasAutomationController::parseNodeKind("mesh") == NodeKind::TrilinearMesh);
    REQUIRE_FALSE(NodeCanvasAutomationController::parseNodeKind("unknown").has_value());

    const auto added = automation.addNode("wave", { 50.f, 70.f });
    REQUIRE(added.succeeded);
    REQUIRE(added.nodeId.isNotEmpty());
    REQUIRE(document.graph().findNode(added.nodeId)->kind == NodeKind::WaveSource);
    REQUIRE_FALSE(automation.addNode("unknown", {}).handled);

    const var diagnostics = automation.inspectOpenGLDiagnostics({ true, 2, 1, {}, {} });
    const auto* object = diagnostics.getDynamicObject();
    REQUIRE(object != nullptr);
    REQUIRE(object->getProperty("schema").toString() == "cycle-v2-opengl-diagnostics.v1");
    REQUIRE((bool) object->getProperty("canvasOpenGlAttached"));
    REQUIRE((int) object->getProperty("contextCreateCount") == 2);
    REQUIRE((int) object->getProperty("contextCloseCount") == 1);
    REQUIRE((int) object->getProperty("panelCount") == 0);
}

std::vector<NodeParameter> curveControls(const Node& node) {
    return node.parameters;
}

}

TEST_CASE("Node editor host follows registered capability and stable identity") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    NullCommands commands;
    NullPresentation presentation;
    NullResources resources;
    EditorStats stats;
    MockFactories factories(stats);
    NodeEditorHost host(parent, commands, presentation, resources, factories);

    Node unsupported = node("plain", NodeKind::Add);
    REQUIRE_FALSE(host.bind(&unsupported, { 0, 0, 100, 80 }));
    REQUIRE_FALSE(host.hasEditor());

    Node first = node("curve-a", NodeKind::Waveshaper);
    REQUIRE(host.bind(&first, { 10, 20, 300, 200 }, 4));
    REQUIRE(stats.creations == 1);
    REQUIRE(stats.bindings == 1);
    REQUIRE(host.component()->getBounds() == Rectangle<int>(10, 20, 300, 200));
    const auto pointerTargets = host.pointerTargetsForAutomation();
    REQUIRE(pointerTargets.size() == 1);
    REQUIRE(pointerTargets.front().id == "mock.control");
    REQUIRE(pointerTargets.front().bounds == Rectangle<float>(15.f, 26.f, 20.f, 10.f));
    DynamicObject automation;
    host.appendAutomationState(automation);
    REQUIRE((bool) automation.getProperty("mock"));

    first.subtitle = "Revised";
    REQUIRE(host.bind(&first, { 20, 30, 320, 220 }, 5));
    REQUIRE(stats.creations == 1);
    REQUIRE(stats.bindings == 2);
    REQUIRE(stats.boundNodeId == "curve-a");

    first.parameters.push_back({ "noise", "Noise", "0.25" });
    REQUIRE(host.bind(&first, { 20, 30, 320, 220 }, 5));
    REQUIRE(stats.creations == 1);
    REQUIRE(stats.bindings == 3);

    Node second = node("curve-b", NodeKind::Waveshaper);
    REQUIRE(host.bind(&second, { 0, 0, 120, 90 }, 5));
    REQUIRE(stats.creations == 2);
    REQUIRE(stats.destructions == 1);
    REQUIRE(host.isEditing("curve-b"));

    host.bind(nullptr, {});
    REQUIRE_FALSE(host.hasEditor());
    REQUIRE(stats.destructions == 2);
}

TEST_CASE("Unison editor exposes structured individual voice state",
        "[cycle-v2][editor][unison][individual]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    RecordingVoiceCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);
    Node unison = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    for (auto& parameter : unison.parameters) {
        if (parameter.id == "mode") {
            parameter.value = "individual";
        }
    }
    unison.model = UnisonNodeModelState::create({
            { 0.25f, 0.f, 0.1f },
            { 0.75f, 1.f, 0.6f }
    }, 2);

    REQUIRE(host.bind(&unison, { 0, 0, 420, 600 }));
    DynamicObject automation;
    host.appendAutomationState(automation);
    const auto* effect = automation.getProperty("effectParameters")
            .getDynamicObject();
    REQUIRE(effect != nullptr);
    REQUIRE(effect->getProperty("mode").toString() == "individual");
    REQUIRE((int) effect->getProperty("voiceCount") == 2);
    REQUIRE((int) effect->getProperty("selectedVoice") == 0);
    REQUIRE(effect->getProperty("modeGroup")
            .getProperty("label", {}).toString() == "Voice mode");
    REQUIRE(effect->getProperty("voiceSelectionGroup")
            .getProperty("label", {}).toString() == "Voice selection");
    REQUIRE(effect->getProperty("parameterGroup")
            .getProperty("label", {}).toString() == "Voice parameters");

    Component* modeSelector = host.component()->findChildWithID("unisonEditor.mode");
    REQUIRE(modeSelector != nullptr);
    auto* groupMode = dynamic_cast<Button*>(
            modeSelector->findChildWithID("unisonEditor.mode.group"));
    auto* individualMode = dynamic_cast<Button*>(
            modeSelector->findChildWithID("unisonEditor.mode.individual"));
    auto* addVoice = dynamic_cast<TextButton*>(
            host.component()->findChildWithID("unisonEditor.addVoice"));
    auto* removeVoice = dynamic_cast<TextButton*>(
            host.component()->findChildWithID("unisonEditor.removeVoice"));
    REQUIRE(groupMode != nullptr);
    REQUIRE(individualMode != nullptr);
    REQUIRE(addVoice != nullptr);
    REQUIRE(removeVoice != nullptr);
    REQUIRE(groupMode->getName() == "Group voice mode");
    REQUIRE(individualMode->getName() == "Individual voice mode");
    REQUIRE(addVoice->getTitle() == "Add Unison voice");
    REQUIRE(removeVoice->getTitle() == "Remove selected Unison voice");

    groupMode->onClick();
    REQUIRE(commands.textParameterId == "mode");
    REQUIRE(commands.textValue == "group");
    DynamicObject groupAutomation;
    host.appendAutomationState(groupAutomation);
    REQUIRE(groupAutomation.getProperty("effectParameters")
            .getProperty("mode", {}).toString() == "group");
    REQUIRE(groupAutomation.getProperty("effectParameters")
            .getProperty("parameterGroup", {})
            .getProperty("label", {}).toString() == "Group parameters");
}

TEST_CASE("Unison group editor keeps jitter inside the expanded panel",
        "[cycle-v2][editor][unison][layout]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    NullCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);
    Node unison = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});

    REQUIRE(host.bind(&unison, { 0, 0, 520, 520 }));
    Component* jitter = host.component()->findChildWithID("unison.jitter.slider");
    REQUIRE(jitter != nullptr);
    REQUIRE(jitter->isVisible());
    REQUIRE(jitter->getBottom() <= host.component()->getHeight() - 18);
}

TEST_CASE("Voice Context hosts semantic controls for every visible property",
        "[cycle-v2][editor][voice-context][properties]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    RecordingVoiceCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);
    Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});

    REQUIRE(host.bind(&voice, { 0, 0, 520, 488 }));
    DynamicObject automation;
    host.appendAutomationState(automation);
    const var state = automation.getProperty("voiceContext");
    REQUIRE(state.getProperty("kind", {}).toString() == "VOICE_CONTEXT");
    const var pitchTimingGroup = state.getProperty("pitchTimingGroup", {});
    const var synthesisGroup = state.getProperty("synthesisGroup", {});
    REQUIRE(pitchTimingGroup.getProperty("label", {}).toString() == "Pitch & timing");
    REQUIRE(synthesisGroup.getProperty("label", {}).toString() == "Synthesis");
    const Rectangle<float> pitchTimingBounds = rectangleProperty(
            pitchTimingGroup, "bounds");
    const Rectangle<float> synthesisBounds = rectangleProperty(
            synthesisGroup, "bounds");
    REQUIRE(pitchTimingBounds.getCentreX()
            == Catch::Approx(host.component()->getWidth() * 0.5f));
    REQUIRE(synthesisBounds.getCentreX()
            == Catch::Approx(pitchTimingBounds.getCentreX()));
    REQUIRE(pitchTimingBounds.getX() == Catch::Approx(24.f));
    REQUIRE(synthesisBounds.getX() == Catch::Approx(pitchTimingBounds.getX()));
    REQUIRE(pitchTimingBounds.getWidth()
            == Catch::Approx(host.component()->getWidth() - 48.f));
    REQUIRE(synthesisBounds.getWidth() == Catch::Approx(pitchTimingBounds.getWidth()));
    REQUIRE(synthesisBounds.getY() > pitchTimingBounds.getBottom());
    REQUIRE(state.getProperty("octave", {}).getProperty("display", {}).toString() == "0");
    REQUIRE(state.getProperty("voiceLength", {}).getProperty("display", {}).toString() == "1 s");
    REQUIRE(state.getProperty("pitch", {}).getProperty("display", {}).toString() == "0 semis");
    REQUIRE(state.getProperty("controlInterval", {}).toString() == "16");
    REQUIRE(state.getProperty("noteMode", {}).toString() == "polyphonic");
    REQUIRE(state.getProperty("envelopeModulation", {}).toString() == "latched");
    REQUIRE(state.getProperty("portamentoSeconds", {}).getProperty("display", {}).toString() == "0.06 s");
    REQUIRE_FALSE((bool) state.getProperty("portamentoEnabled", {}));
    REQUIRE_FALSE((bool) state.getProperty("glideTimeEnabled", {}));
    REQUIRE(!(bool) state.getProperty("pitchIndependentSpectralControl", {}));
    REQUIRE((int) state.getProperty("octave", {}).getProperty("usableTrackWidth", {}) >= 140);
    REQUIRE((int) state.getProperty("voiceLength", {}).getProperty("usableTrackWidth", {}) >= 140);
    REQUIRE((int) state.getProperty("pitch", {}).getProperty("usableTrackWidth", {}) >= 140);
    const Rectangle<float> octaveTrack = rectangleProperty(
            state.getProperty("octave", {}),
            "track");
    const Rectangle<float> voiceLengthTrack = rectangleProperty(
            state.getProperty("voiceLength", {}),
            "track");
    const Rectangle<float> pitchTrack = rectangleProperty(
            state.getProperty("pitch", {}),
            "track");
    REQUIRE(octaveTrack.getX() == Catch::Approx(voiceLengthTrack.getX()));
    REQUIRE(octaveTrack.getX() == Catch::Approx(pitchTrack.getX()));
    REQUIRE(octaveTrack.getWidth() == Catch::Approx(voiceLengthTrack.getWidth()));
    REQUIRE(octaveTrack.getWidth() == Catch::Approx(pitchTrack.getWidth()));
    REQUIRE(pitchTimingBounds.getBottom() <= octaveTrack.getY());

    auto* pitchValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "voiceContextEditor.pitch.value"));
    REQUIRE(pitchValue != nullptr);
    pitchValue->setText("7 semitones", sendNotificationSync);
    REQUIRE(commands.begins == 1);
    REQUIRE(commands.updates == 1);
    REQUIRE(commands.ends == 1);
    REQUIRE(commands.numericValues.back() == Catch::Approx(7.f));
    pitchValue->setText("7.5 semitones", sendNotificationSync);
    REQUIRE(commands.updates == 1);

    auto* oversamplingSelector = host.component()->findChildWithID(
            "voiceContextEditor.oversampling");
    REQUIRE(oversamplingSelector != nullptr);
    REQUIRE(synthesisBounds.getBottom() <= oversamplingSelector->getY());
    for (const String& factor : { String("1x"), String("2x"), String("4x"), String("8x") }) {
        auto* option = dynamic_cast<TextButton*>(oversamplingSelector->findChildWithID(
                "voiceContextEditor.oversampling." + factor));
        REQUIRE(option != nullptr);
        option->onClick();
        REQUIRE(commands.textParameterId == "oversampling");
        REQUIRE(commands.textValue == factor);
    }
    auto* controlIntervalSelector = host.component()->findChildWithID(
            "voiceContextEditor.controlInterval");
    REQUIRE(controlIntervalSelector != nullptr);
    for (const String& interval : { String("16"), String("64"), String("256"), String("1024") }) {
        auto* option = dynamic_cast<TextButton*>(controlIntervalSelector->findChildWithID(
                "voiceContextEditor.controlInterval." + interval));
        REQUIRE(option != nullptr);
        option->onClick();
        REQUIRE(commands.textParameterId == "controlInterval");
        REQUIRE(commands.textValue == interval);
    }
    auto* portamento = dynamic_cast<ToggleButton*>(host.component()->findChildWithID(
            "voiceContextEditor.portamento"));
    REQUIRE(portamento != nullptr);
    REQUIRE_FALSE(portamento->isEnabled());
    auto* noteMode = host.component()->findChildWithID("voiceContextEditor.noteMode");
    REQUIRE(noteMode != nullptr);
    auto* legato = dynamic_cast<TextButton*>(noteMode->findChildWithID(
            "voiceContextEditor.noteMode.legato"));
    REQUIRE(legato != nullptr);
    legato->onClick();
    REQUIRE(commands.textParameterId == "noteMode");
    REQUIRE(commands.textValue == "legato");
    REQUIRE(portamento->isEnabled());
    portamento->setToggleState(true, dontSendNotification);
    portamento->onClick();
    REQUIRE(commands.immediateParameterId == "portamento");
    REQUIRE(commands.immediateValue == Catch::Approx(1.f));

    auto* pitchIndependentSpectralControl = dynamic_cast<ToggleButton*>(
            host.component()->findChildWithID(
                    "voiceContextEditor.pitchIndependentSpectralControl"));
    REQUIRE(pitchIndependentSpectralControl != nullptr);
    REQUIRE(pitchIndependentSpectralControl->getBottom()
            <= host.component()->getHeight() - 4);
    pitchIndependentSpectralControl->setToggleState(true, dontSendNotification);
    pitchIndependentSpectralControl->onClick();
    REQUIRE(commands.immediateParameterId == "pitchIndependentSpectralControl");
    REQUIRE(commands.immediateValue == Catch::Approx(1.f));

    auto* voiceLengthValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "voiceContextEditor.voiceLength.value"));
    REQUIRE(voiceLengthValue != nullptr);
    voiceLengthValue->setText("2 s", sendNotificationSync);
    REQUIRE(commands.activeParameterId.isEmpty());
    REQUIRE(commands.immediateParameterId == "pitchIndependentSpectralControl");
    REQUIRE(commands.numericValues.back()
            == Catch::Approx(CycleDsp::voiceLengthUnitValue(2.0)).margin(0.0001));
    auto* voiceLength = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "voiceContextEditor.voiceLength"));
    REQUIRE(voiceLength != nullptr);
    REQUIRE(voiceLength->keyPressed(KeyPress(
            KeyPress::rightKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(commands.immediateParameterId == "pitchIndependentSpectralControl");
    REQUIRE(commands.numericValues.back()
            == Catch::Approx(CycleDsp::voiceLengthUnitValue(2.01)).margin(0.0001));

    auto* glideTime = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "voiceContextEditor.portamentoSeconds"));
    REQUIRE(glideTime != nullptr);
    REQUIRE(glideTime->isEnabled());
    auto* glideValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "voiceContextEditor.portamentoSeconds.value"));
    REQUIRE(glideValue != nullptr);
    glideValue->setText("0.125 s", sendNotificationSync);
    REQUIRE(commands.numericValues.back() == Catch::Approx(.125f));
    REQUIRE(glideTime->keyPressed(KeyPress(KeyPress::rightKey, ModifierKeys::shiftModifier, 0)));
    REQUIRE(commands.numericValues.back() == Catch::Approx(.126f));
    const auto updates = commands.updates;
    glideValue->setText("0.6 s", sendNotificationSync);
    REQUIRE(commands.updates == updates);
    glideValue->setText("0 s", sendNotificationSync);
    REQUIRE(commands.numericValues.back() == Catch::Approx(0.f));
    glideValue->setText("0.5 s", sendNotificationSync);
    REQUIRE(commands.numericValues.back() == Catch::Approx(.5f));

    DynamicObject updated;
    host.appendAutomationState(updated);
    const var glideState = updated.getProperty("voiceContext").getProperty("portamentoSeconds", {});
    const Rectangle<float> glideTrack = rectangleProperty(glideState, "track");
    REQUIRE(glideTrack.getX() == Catch::Approx(octaveTrack.getX()));
    REQUIRE(glideTrack.getWidth() == Catch::Approx(octaveTrack.getWidth()));
    REQUIRE(glideTrack.getCentreY() < synthesisBounds.getY());
    REQUIRE(noteMode->getX() == 24 + PropertyControlMetrics::labelWidth + PropertyControlMetrics::inlineGap);
    REQUIRE(noteMode->getRight() == host.component()->getWidth() - 24);
}

TEST_CASE("Voice Context articulation controls commit semantic policies and one glide gesture",
        "[cycle-v2][editor][voice-context][articulation][gesture]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
    NodeEditorHost host(owner, commands, presentation, resources);
    REQUIRE(host.bind(document.graph().findNode("voice"), { 0, 0, 520, 488 }));

    const auto select = [&](const String& selectorId, const String& optionId) {
        auto* selector = host.component()->findChildWithID(selectorId);
        REQUIRE(selector != nullptr);
        auto* option = dynamic_cast<TextButton*>(selector->findChildWithID(optionId));
        REQUIRE(option != nullptr);
        option->onClick();
    };
    select("voiceContextEditor.noteMode", "voiceContextEditor.noteMode.legato");
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "noteMode") == "legato");
    select("voiceContextEditor.envelopeModulation", "voiceContextEditor.envelopeModulation.live");
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "envelopeModulation") == "live");
    auto* portamento = dynamic_cast<ToggleButton*>(host.component()->findChildWithID(
            "voiceContextEditor.portamento"));
    REQUIRE(portamento != nullptr);
    REQUIRE(portamento->isEnabled());
    portamento->setToggleState(true, dontSendNotification);
    portamento->onClick();
    REQUIRE(NodeParameterMap(*document.graph().findNode("voice")).boolValue("portamento"));

    auto* glideTime = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "voiceContextEditor.portamentoSeconds"));
    REQUIRE(glideTime != nullptr);
    REQUIRE(glideTime->isEnabled());
    glideTime->onDragStart();
    glideTime->setValue(.12, sendNotificationSync);
    glideTime->setValue(.22, sendNotificationSync);
    REQUIRE(NodeParameterMap(*document.graph().findNode("voice")).floatValue("portamentoSeconds")
            == Catch::Approx(.06f));
    REQUIRE(NodeParameterMap(*dispatcher.editingGraph().findNode("voice")).floatValue("portamentoSeconds")
            == Catch::Approx(.22f));
    glideTime->onDragEnd();
    REQUIRE(NodeParameterMap(*document.graph().findNode("voice")).floatValue("portamentoSeconds")
            == Catch::Approx(.22f));
    REQUIRE(document.undo());
    REQUIRE(NodeParameterMap(*document.graph().findNode("voice")).floatValue("portamentoSeconds")
            == Catch::Approx(.06f));
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.gestureCommits == 1);

    REQUIRE(host.bind(document.graph().findNode("voice"), { 0, 0, 520, 488 }));
    select("voiceContextEditor.noteMode", "voiceContextEditor.noteMode.polyphonic");
    REQUIRE_FALSE(portamento->isEnabled());
    REQUIRE_FALSE(glideTime->isEnabled());
    select("voiceContextEditor.envelopeModulation", "voiceContextEditor.envelopeModulation.latched");
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "envelopeModulation") == "latched");
}

TEST_CASE("Delay and Reverb own shared semantic property rows",
        "[cycle-v2][editor][delay][reverb][properties]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    NullCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);

    const auto controlWithId = [](const DynamicObject& automation, const String& id) {
        const Array<var>* controls = automation.getProperty("effectParameters")
                .getProperty("controls", {})
                .getArray();
        REQUIRE(controls != nullptr);
        for (const var& control : *controls) {
            if (control.getProperty("id", {}).toString() == id) {
                return control;
            }
        }
        return var();
    };

    Node delay = GraphNodeFactory().createNode(NodeKind::Delay, "delay", {});
    REQUIRE(host.bind(&delay, { 0, 0, 520, 520 }));
    DynamicObject delayAutomation;
    host.appendAutomationState(delayAutomation);
    REQUIRE(delayAutomation.getProperty("effectParameters")
            .getProperty("kind", {}).toString() == "DELAY");
    const var delayState = delayAutomation.getProperty("effectParameters");
    REQUIRE(delayState.getProperty("echoGroup", {})
            .getProperty("label", {}).toString() == "Echo");
    REQUIRE(delayState.getProperty("stereoOutputGroup", {})
            .getProperty("label", {}).toString() == "Stereo / output");
    const var time = controlWithId(delayAutomation, "time");
    REQUIRE(time.getProperty("readout", {}).toString() == "1 beat");
    REQUIRE((bool) time.getProperty("compact", {}));
    REQUIRE((int) time.getProperty("usableTrackWidth", {}) >= 140);
    const var panCycle = controlWithId(delayAutomation, "spinIters");
    REQUIRE(panCycle.getProperty("readout", {}).toString() == String::fromUTF8("1×"));
    auto* timeValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "delayEditor.time.value"));
    REQUIRE(timeValue != nullptr);
    timeValue->setText("2 beats", sendNotificationSync);
    DynamicObject editedDelay;
    host.appendAutomationState(editedDelay);
    REQUIRE(controlWithId(editedDelay, "time").getProperty("value", {})
            == Catch::Approx(CycleDsp::delayUnitValueForBeats(2.0, 4)));
    auto* timeSlider = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "delayEditor.time"));
    auto* panCycleSlider = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "delayEditor.spinIters"));
    REQUIRE(timeSlider != nullptr);
    REQUIRE(panCycleSlider != nullptr);
    const Rectangle<float> timeTrack = propertySliderTrackBounds(
            timeSlider->getLocalBounds().toFloat());
    REQUIRE(propertySliderValuePosition(
                    *timeSlider,
                    CycleDsp::delayUnitValueForBeats(2.0, 4))
            == Catch::Approx(timeTrack.getX()
                    + timeTrack.getWidth()
                            * CycleDsp::delayUnitValueForBeats(2.0, 4)));

    const Rectangle<float> panCycleTrack = propertySliderTrackBounds(
            panCycleSlider->getLocalBounds().toFloat());
    float previousPosition = propertySliderValuePosition(
            *panCycleSlider,
            CycleDsp::delaySpinUnitValueForIterations(1));
    REQUIRE(previousPosition == Catch::Approx(panCycleTrack.getX()));
    for (int iterations = 2; iterations <= 12; ++iterations) {
        const float position = propertySliderValuePosition(
                *panCycleSlider,
                CycleDsp::delaySpinUnitValueForIterations(iterations));
        REQUIRE(position - previousPosition
                == Catch::Approx(panCycleTrack.getWidth() / 11.f));
        previousPosition = position;
    }
    REQUIRE(previousPosition == Catch::Approx(panCycleTrack.getRight()));

    Node reverb = GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", {});
    REQUIRE(host.bind(&reverb, { 0, 0, 520, 520 }));
    DynamicObject reverbAutomation;
    host.appendAutomationState(reverbAutomation);
    REQUIRE(reverbAutomation.getProperty("effectParameters")
            .getProperty("kind", {}).toString() == "REVERB");
    const var reverbState = reverbAutomation.getProperty("effectParameters");
    REQUIRE(reverbState.getProperty("spaceGroup", {})
            .getProperty("label", {}).toString() == "Space");
    REQUIRE(reverbState.getProperty("toneOutputGroup", {})
            .getProperty("label", {}).toString() == "Tone / output");
    const var size = controlWithId(reverbAutomation, "size");
    REQUIRE(size.getProperty("readout", {}).toString() == "0.74 s");
    REQUIRE((bool) size.getProperty("compact", {}));
    REQUIRE((int) size.getProperty("usableTrackWidth", {}) >= 140);
    auto* sizeSlider = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "reverbEditor.size"));
    REQUIRE(sizeSlider != nullptr);
    REQUIRE(sizeSlider->snapValue(0.61, Slider::absoluteDrag)
            == Catch::Approx(CycleDsp::reverbSizeUnitValueForStep(4)));
    const var wet = controlWithId(reverbAutomation, "wet");
    REQUIRE(wet.getProperty("readout", {}).toString() == "40%");
    auto* sizeValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "reverbEditor.size.value"));
    REQUIRE(sizeValue != nullptr);
    sizeValue->setText("1.49 s", sendNotificationSync);
    DynamicObject editedReverb;
    host.appendAutomationState(editedReverb);
    REQUIRE(controlWithId(editedReverb, "size").getProperty("readout", {}).toString()
            == "1.5 s");
}

TEST_CASE("Expanded effects share one enable action placement",
        "[cycle-v2][editor][effects][chrome]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    NullCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);

    struct EffectEditorCase {
        NodeKind kind;
        String nodeId;
        String controlId;
        Rectangle<int> bounds;
    };
    const std::array<EffectEditorCase, 4> cases {{
            { NodeKind::Delay, "delay", "delayEditor.enabled", { 0, 0, 520, 520 } },
            { NodeKind::Reverb, "reverb", "reverbEditor.enabled", { 0, 0, 520, 520 } },
            { NodeKind::Equalizer, "eq", "equalizerEditor.enabled", { 0, 0, 760, 650 } },
            { NodeKind::Unison, "unison", "unisonEditor.enabled", { 0, 0, 520, 520 } }
    }};

    for (const auto& effect : cases) {
        Node node = GraphNodeFactory().createNode(effect.kind, effect.nodeId, {});
        REQUIRE(host.bind(&node, effect.bounds));
        const auto* enabled = dynamic_cast<EffectEnableButton*>(
                host.component()->findChildWithID(effect.controlId));
        REQUIRE(enabled != nullptr);
        REQUIRE(enabled->getBounds()
                == fullEditorHeaderLayout(host.component()->getLocalBounds(), true).enabled);
    }
}

TEST_CASE("Property rows collapse nested slider notifications into one gesture",
        "[cycle-v2][editor][properties][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    RecordingParameterCommands commands;
    NodePropertySliderRow row(owner, commands, "feedback", "Feedback");
    row.bind("delay", 0.6);

    row.slider.onDragStart();
    row.slider.onDragStart();
    row.slider.setValue(0.5, sendNotificationSync);
    row.slider.onDragEnd();
    row.slider.onDragEnd();

    REQUIRE(commands.begins == 1);
    REQUIRE(commands.updates == 1);
    REQUIRE(commands.ends == 1);
    REQUIRE(commands.values == std::vector<float> { 0.6f, 0.5f });
}

TEST_CASE("Equalizer retains paired columns with semantic shared rows",
        "[cycle-v2][editor][equalizer][properties]") {
    ScopedJuceInitialiser_GUI juce;
    Component parent;
    RecordingVoiceCommands commands;
    NullPresentation presentation;
    NullResources resources;
    NodeEditorHost host(parent, commands, presentation, resources);
    Node equalizer = GraphNodeFactory().createNode(NodeKind::Equalizer, "eq", {});

    REQUIRE(host.bind(&equalizer, { 0, 0, 760, 650 }));
    DynamicObject automation;
    host.appendAutomationState(automation);
    const Array<var>* controls = automation.getProperty("effectParameters")
            .getProperty("controls", {})
            .getArray();
    REQUIRE(controls != nullptr);
    REQUIRE(controls->size() == 10);
    REQUIRE(controls->getReference(0).getProperty("readout", {}).toString() == "0 dB");
    REQUIRE(controls->getReference(1).getProperty("readout", {}).toString() == "60 Hz");
    REQUIRE((bool) controls->getReference(0).getProperty("compact", {}));
    REQUIRE((int) controls->getReference(0).getProperty("usableTrackWidth", {}) >= 140);
    const var effectState = automation.getProperty("effectParameters");
    REQUIRE(effectState.getProperty("gainGroup", {})
            .getProperty("label", {}).toString() == "Gain");
    REQUIRE(effectState.getProperty("frequencyGroup", {})
            .getProperty("label", {}).toString() == "Frequency");
    const var processing = effectState.getProperty("processingScope", {});
    REQUIRE(processing.getProperty("scope", {}).toString() == "voice");
    REQUIRE(rectangleProperty(processing, "bounds").getWidth() >= 112.f);
    REQUIRE(rectangleProperty(processing, "bounds").getHeight() >= 28.f);
    REQUIRE(effectState.getProperty("processingGroup", {})
            .getProperty("label", {}).toString() == "PROCESSING");

    auto* scopeSelector = dynamic_cast<ProcessingScopeSelector*>(
            host.component()->findChildWithID("equalizerEditor.processingScope"));
    REQUIRE(scopeSelector != nullptr);
    auto* globalScope = dynamic_cast<Button*>(scopeSelector->findChildWithID(
            "equalizerEditor.processingScope.global"));
    REQUIRE(globalScope != nullptr);
    globalScope->onClick();
    REQUIRE(commands.textParameterId == "processingScope");
    REQUIRE(commands.textValue == "global");

    auto* gainValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "equalizerEditor.band1Gain.value"));
    auto* frequencyValue = dynamic_cast<Label*>(host.component()->findChildWithID(
            "equalizerEditor.band1Frequency.value"));
    REQUIRE(gainValue != nullptr);
    REQUIRE(frequencyValue != nullptr);
    gainValue->setText("+6 dB", sendNotificationSync);
    frequencyValue->setText("1.5 kHz", sendNotificationSync);

    DynamicObject edited;
    host.appendAutomationState(edited);
    const Array<var>* editedControls = edited.getProperty("effectParameters")
            .getProperty("controls", {})
            .getArray();
    REQUIRE(editedControls != nullptr);
    REQUIRE(editedControls->getReference(0).getProperty("value", {})
            == Catch::Approx(CycleDsp::equalizerGainUnitValue(6.f)));
    REQUIRE(editedControls->getReference(1).getProperty("value", {})
            == Catch::Approx(CycleDsp::equalizerFrequencyUnitValue(1500.f)));
}

TEST_CASE("Canvas automation inspection is semantic and side effect free",
        "[cycle-v2][canvas][automation]") {
    ScopedJuceInitialiser_GUI juce;
    Component canvas;
    canvas.setBounds(0, 0, 1200, 800);

    NullCommands commands;
    NullPresentation editorPresentation;
    NullResources resources;
    EditorStats editorStats;
    MockFactories factories(editorStats);
    NodeEditorHost host(canvas, commands, editorPresentation, resources, factories);

    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            { 240.f, 180.f }));
    REQUIRE(GuideGraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(graph.assignGuideCurve({
            "guide1",
            "mesh",
            { 0, GuideCurveField::Amplitude }
    }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher graphCommands(document);
    NodeEditorCommandService editorCommands(
            canvas,
            document,
            graphCommands,
            editorPresentation,
            resources);
    NodePreviewResources previewResources(editorCommands);
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(document.graph(), document.revision()));

    NodeCanvasViewport viewport;
    viewport.setBounds(canvas.getLocalBounds().toFloat());
    NodeCanvasAutomationInspector inspector({
            canvas,
            document,
            presentation,
            viewport,
            host
    });
    NodeCanvasAutomationPresentation state {
            "mesh",
            "mesh",
            "Opened editor: mesh",
            -1
    };
    state.guideDock.guideShelfBounds = { 0.f, 0.f, 260.f, 800.f };
    state.guideDock.guideMinimizeBounds = { 12.f, 620.f, 18.f, 18.f };
    state.guideDock.addGuideBounds = { 566.f, 618.f, 22.f, 22.f };
    state.guideDock.expandedGuideId = "guide1";
    state.guideDock.guideEditorBounds = { 36.f, 24.f, 1128.f, 562.f };
    state.guideDock.guideEditorTargets.push_back({
            "guideEditor.noise",
            { 900.f, 140.f, 148.f, 30.f }
    });
    state.guideDock.guideTiles.push_back({ "guide1", { 12.f, 652.f, 220.f, 136.f } });
    const uint64_t documentRevision = document.revision();
    const uint64_t presentationRevision = presentation.revision();
    const uint64_t viewportRevision = viewport.getRevision();
    REQUIRE(previewResources.findTrimeshWidget("mesh") == nullptr);

    const var pointerInspection = inspector.inspectPointerTargets(state);
    const auto* pointerObject = pointerInspection.getDynamicObject();
    REQUIRE(pointerObject != nullptr);
    REQUIRE(pointerObject->getProperty("schema").toString() == "cycle-v2-pointer-targets.v1");

    const Array<var>* targets = pointerObject->getProperty("targets").getArray();
    REQUIRE(targets != nullptr);

    auto targetWithId = [targets](const String& id) -> const DynamicObject* {
        const auto match = std::find_if(targets->begin(), targets->end(), [&](const var& targetValue) {
            const auto* target = targetValue.getDynamicObject();
            return target != nullptr && target->getProperty("id").toString() == id;
        });
        return match != targets->end() ? match->getDynamicObject() : nullptr;
    };

    REQUIRE(targetWithId("node:mesh") != nullptr);
    REQUIRE(targetWithId("guideShelf") != nullptr);
    REQUIRE(targetWithId("guideDock.collapse") == nullptr);
    REQUIRE(targetWithId("guideDock.resize") == nullptr);
    REQUIRE(targetWithId("guideShelf.minimize") != nullptr);
    REQUIRE(targetWithId("guideEditor.noise") != nullptr);
    REQUIRE(targetWithId("spyShelf.minimize") == nullptr);
    REQUIRE(targetWithId("guideShelf.add") != nullptr);
    REQUIRE(targetWithId("guide:guide1") != nullptr);
    REQUIRE(targetWithId("guideEditor:guide1") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.panel3D") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshMorphRail.yellow") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexParameter.vertex.phase") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexGuideGain.guideGain.phase") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexGuideGain.guideGain.time") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexGuideGain.guideGain.red") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexGuideGain.guideGain.blue") != nullptr);
    const DynamicObject* expandedTarget = targetWithId("expanded:mesh");
    REQUIRE(expandedTarget != nullptr);
    REQUIRE_FALSE((bool) expandedTarget->getProperty("nativeReady"));
    REQUIRE(expandedTarget->getProperty("screenBounds").isVoid());

    const var snapshot = inspector.exportState(state);
    const auto* snapshotObject = snapshot.getDynamicObject();
    REQUIRE(snapshotObject != nullptr);
    REQUIRE((int) snapshotObject->getProperty("nodeCount") == 1);
    REQUIRE((int) snapshotObject->getProperty("guideCount") == 1);
    REQUIRE((int) snapshotObject->getProperty("guideAssignmentCount") == 1);
    REQUIRE(snapshotObject->getProperty("selectedNodeId").toString() == "mesh");
    REQUIRE_FALSE((bool) snapshotObject->getProperty("documentDirty"));
    REQUIRE(snapshotObject->hasProperty("documentFile"));
    REQUIRE(snapshotObject->getProperty("expandedGuideId").toString() == "guide1");
    const Array<var>* guides = snapshotObject->getProperty("guides").getArray();
    REQUIRE(guides != nullptr);
    REQUIRE(guides->size() == 1);
    REQUIRE(guides->getReference(0).getProperty("id", {}).toString() == "guide1");
    const Array<var>* assignments = snapshotObject->getProperty("guideAssignments").getArray();
    REQUIRE(assignments != nullptr);
    REQUIRE(assignments->size() == 1);
    REQUIRE(assignments->getReference(0).getProperty("guideId", {}).toString() == "guide1");
    REQUIRE(assignments->getReference(0).getProperty("target", {})
            .getProperty("field", {}).toString() == "amp");
    REQUIRE(inspector.exportGraphJson() == document.toJson());

    REQUIRE(editorStats.creations == 0);
    REQUIRE(previewResources.findTrimeshWidget("mesh") == nullptr);
    REQUIRE(document.revision() == documentRevision);
    REQUIRE(presentation.revision() == presentationRevision);
    REQUIRE(viewport.getRevision() == viewportRevision);
}

TEST_CASE("Curve editor bindings own continuous and discrete edit lifecycle") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(NodeKind::Waveshaper);
    LifecycleCurveEditor editor(widget);
    RecordingCurveDelegate delegate;

    editor.setDelegate(&delegate);
    editor.setNode(GraphNodeFactory().createNode(NodeKind::Waveshaper, "curve", {}));

    editor.slider.slider.onDragStart();
    editor.slider.slider.setValue(0.73, sendNotificationSync);
    editor.slider.slider.onDragEnd();
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    editor.toggle.button.onClick();
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    editor.menu.addItem("Four", 4);
    editor.menu.setSelectedId(4, sendNotificationSync);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    editor.action.onClick();
    REQUIRE(editor.actionPerformed);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
}

TEST_CASE("Curve editor bindings resynchronize reused preset node identities",
          "[cycle-v2][node-editor-host][presets]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    const File presetDirectory = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content")
            .getChildFile("presets");
    const NodeGraph baroque = GraphSerializer().fromJsonString(
            presetDirectory.getChildFile("baroque-flute.cyclegraph").loadFileAsString());
    const NodeGraph stengah = GraphSerializer().fromJsonString(
            presetDirectory.getChildFile("stengah.cyclegraph").loadFileAsString());
    const GuideCurveResource* baroqueGuide = baroque.findGuideCurve("guide1");
    const GuideCurveResource* stengahGuide = stengah.findGuideCurve("guide1");
    REQUIRE(baroqueGuide != nullptr);
    REQUIRE(stengahGuide != nullptr);

    CurveEditorWidget widget(true);
    GuideCurveEditorComponent editor(widget);
    editor.setBounds(0, 0, 640, 400);
    editor.setGuideResource(*baroqueGuide);
    REQUIRE(widget.vertexCountForAutomation() == 4);
    REQUIRE(static_cast<double>(editor.automationState().getProperty("noise", {}))
            == Catch::Approx(0.76562));
    editor.setGuideResource(*stengahGuide);
    const auto stengahModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
            stengahGuide->model);
    REQUIRE(stengahModel != nullptr);
    REQUIRE(stengahModel->flatCurve() != nullptr);
    REQUIRE(widget.vertexCountForAutomation()
            == stengahModel->flatCurve()->getMesh().getNumVerts());
    REQUIRE(static_cast<double>(editor.automationState().getProperty("noise", {}))
            == Catch::Approx(0.0025));

    widget.syncFromGuideResource(*baroqueGuide);
    REQUIRE(static_cast<double>(widget.automationState().getProperty("firstControl", {}))
            == Catch::Approx(0.76562));
    GuideCurveResource revisedGuide = *stengahGuide;
    revisedGuide.dcOffset = 0.25f;
    revisedGuide.phase = 0.75f;
    widget.syncFromGuideResource(revisedGuide);
    const var widgetState = widget.automationState();
    REQUIRE(static_cast<double>(widgetState.getProperty("firstControl", {}))
            == Catch::Approx(0.0025));
    REQUIRE(static_cast<double>(widgetState.getProperty("secondControl", {}))
            == Catch::Approx(0.25));
    REQUIRE(static_cast<double>(widgetState.getProperty("thirdControl", {}))
            == Catch::Approx(0.75));
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Guide editor uses compact host and control layout",
        "[cycle-v2][node-editor-host][guides]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(true);
    GuideCurveEditorComponent editor(widget);
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.noise = 0.76562f;
    guide.model = createDefaultGuideCurveModel();

    const Rectangle<float> host = GuideCurveEditorComponent::preferredHostBounds(
            { 0.f, 0.f, 1600.f, 900.f });
    REQUIRE(host.getHeight() == 476.f);
    REQUIRE(host.getWidth() == 1265.f);
    REQUIRE(host.getCentreX() == 800.f);
    REQUIRE(host.getCentreY() == 450.f);

    editor.setBounds(host.toNearestInt());
    editor.setGuideResource(guide);

    int sliderCount = 0;
    int textButtonCount = 0;
    int emptyToggleCount = 0;
    int enabledLabelCount = 0;
    int xRandomnessLabelCount = 0;
    int yRandomnessLabelCount = 0;
    int valueLabelCount = 0;
    int lowestControlBottom = 0;
    for (int index = 0; index < editor.getNumChildComponents(); ++index) {
        Component* child = editor.getChildComponent(index);
        if (auto* slider = dynamic_cast<Slider*>(child)) {
            ++sliderCount;
            REQUIRE(slider->getTextBoxPosition() == Slider::NoTextBox);
            REQUIRE(slider->getComponentID().startsWith("guideEditor."));
            lowestControlBottom = jmax(lowestControlBottom, slider->getBottom());
        } else if (dynamic_cast<TextButton*>(child) != nullptr) {
            ++textButtonCount;
            lowestControlBottom = jmax(lowestControlBottom, child->getBottom());
        } else if (auto* toggle = dynamic_cast<ToggleButton*>(child)) {
            emptyToggleCount += toggle->getButtonText().isEmpty() ? 1 : 0;
            lowestControlBottom = jmax(lowestControlBottom, toggle->getBottom());
        } else if (auto* label = dynamic_cast<Label*>(child)) {
            enabledLabelCount += label->getText() == "Enabled" ? 1 : 0;
            xRandomnessLabelCount += label->getText() == "X Randomness" ? 1 : 0;
            yRandomnessLabelCount += label->getText() == "Y Randomness" ? 1 : 0;
            valueLabelCount += label->getComponentID().endsWith(".value") ? 1 : 0;
        }
    }
    REQUIRE(sliderCount == 3);
    REQUIRE(textButtonCount == 2);
    REQUIRE(emptyToggleCount == 1);
    REQUIRE(enabledLabelCount == 0);
    REQUIRE(xRandomnessLabelCount == 1);
    REQUIRE(yRandomnessLabelCount == 1);
    REQUIRE(valueLabelCount == 3);
    REQUIRE(lowestControlBottom < 300);

    const var state = editor.automationState();
    const Rectangle<float> headerActionBounds = rectangleProperty(
            state,
            "headerActionBounds");
    const var noiseLayout = state.getProperty("noiseLayout", {});
    REQUIRE(noiseLayout.getProperty("display", {}).toString() == "77%");
    REQUIRE(static_cast<int>(noiseLayout.getProperty("usableTrackWidth", {}))
            >= PropertyControlMetrics::minimumUsableTrackWidth);
    REQUIRE(static_cast<bool>(noiseLayout.getProperty("compact", {})));
    const Rectangle<float> noiseLabel = rectangleProperty(noiseLayout, "label");
    const Rectangle<float> noiseSlider = rectangleProperty(noiseLayout, "slider");
    const Rectangle<float> noiseTrack = rectangleProperty(noiseLayout, "track");
    const Rectangle<float> noiseValue = rectangleProperty(noiseLayout, "value");
    REQUIRE(noiseValue.getWidth() == 48.f);
    REQUIRE(noiseValue.getX() - noiseLabel.getRight() == 4.f);
    REQUIRE(noiseLabel.getY() == noiseValue.getY());
    REQUIRE(noiseSlider.getWidth() == 324.f);
    REQUIRE(noiseTrack.getY() - noiseLabel.getBottom() <= 10.f);
    const Rectangle<float> panelBounds = rectangleProperty(state, "panelBounds");
    const Rectangle<float> controlBounds = rectangleProperty(state, "controlBounds");
    REQUIRE(controlBounds.getWidth() == 336.f);
    REQUIRE(panelBounds.getWidth() == 905.f);
    const float leftTrackInset = noiseTrack.getX() - controlBounds.getX();
    const float rightTrackInset = (float) editor.getWidth() - noiseTrack.getRight();
    REQUIRE(leftTrackInset == rightTrackInset);
    const Rectangle<float> dcOffsetTrack = rectangleProperty(
            state.getProperty("dcOffsetLayout", {}), "track");
    const Rectangle<float> phaseTrack = rectangleProperty(
            state.getProperty("phaseLayout", {}), "track");
    REQUIRE(dcOffsetTrack.getX() == noiseTrack.getX());
    REQUIRE(dcOffsetTrack.getRight() == noiseTrack.getRight());
    REQUIRE(phaseTrack.getX() == noiseTrack.getX());
    REQUIRE(phaseTrack.getRight() == noiseTrack.getRight());
    const var panelState = state.getProperty("panelState", {});
    const var zoom = panelState.getProperty("zoom", {});
    REQUIRE(static_cast<double>(zoom.getProperty("x", {}))
            == Catch::Approx(0.0));
    REQUIRE(static_cast<double>(zoom.getProperty("w", {}))
            == Catch::Approx(1.0));
    const Array<var>* majorGridLines = panelState
            .getProperty("verticalMajorGridLines", {})
            .getArray();
    REQUIRE(majorGridLines != nullptr);
    REQUIRE(majorGridLines->size() == 8);
    const double expectedGridSpacing = 0.9 / 8.0;
    for (int index = 0; index < majorGridLines->size(); ++index) {
        REQUIRE(static_cast<double>((*majorGridLines)[index]
                .getProperty("domainX", {}))
                == Catch::Approx(0.05 + expectedGridSpacing * index));
    }
    REQUIRE(static_cast<double>((*majorGridLines)[4].getProperty("panelX", {}))
            == Catch::Approx(panelBounds.getWidth() * 0.5));
    REQUIRE(panelBounds.getWidth() * 0.95
                    - static_cast<double>(majorGridLines->getLast()
                            .getProperty("panelX", {}))
            == Catch::Approx(panelBounds.getWidth() * expectedGridSpacing));
    REQUIRE(state.getProperty("heatmapSectionLabel", {}).toString() == "Guide image");
    REQUIRE_FALSE(static_cast<bool>(state.getProperty("heatmapSublabelVisible", {})));
    auto* loadImage = dynamic_cast<TextButton*>(
            editor.findChildWithID("guideEditor.loadImage"));
    auto* clearImage = dynamic_cast<TextButton*>(
            editor.findChildWithID("guideEditor.clearImage"));
    REQUIRE(loadImage != nullptr);
    REQUIRE(clearImage != nullptr);
    REQUIRE(loadImage->getButtonText() == "Load");
    REQUIRE(clearImage->getButtonText() == "Clear");
    REQUIRE(loadImage->getHeight() == 24);
    REQUIRE(clearImage->getHeight() == loadImage->getHeight());
    REQUIRE(clearImage->getY() == loadImage->getY());
    REQUIRE(loadImage->getWidth() == 72);
    REQUIRE_FALSE(clearImage->isEnabled());
    auto* enabled = dynamic_cast<EffectEnableButton*>(
            editor.findChildWithID("guideEditor.enabled"));
    REQUIRE(enabled != nullptr);
    REQUIRE(enabled->getBounds().toFloat() == headerActionBounds);
    REQUIRE(headerActionBounds == embeddedEditorHeaderLayout(
            editor.getLocalBounds().toFloat(), true).enabled);
}

TEST_CASE("Guide property keyboard edits use one complete Curve transaction",
        "[cycle-v2][node-editor-host][guides][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(true);
    GuideCurveEditorComponent editor(widget);
    RecordingCurveDelegate delegate;
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.model = createDefaultGuideCurveModel();

    editor.setDelegate(&delegate);
    editor.setBounds(GuideCurveEditorComponent::preferredHostBounds(
            { 0.f, 0.f, 1200.f, 800.f }).toNearestInt());
    editor.setGuideResource(guide);

    auto* enabled = dynamic_cast<EffectEnableButton*>(
            editor.findChildWithID("guideEditor.enabled"));
    REQUIRE(enabled != nullptr);
    REQUIRE(enabled->getWantsKeyboardFocus());
    REQUIRE(enabled->getTooltip() == "Enable or disable this Guide curve");
    enabled->setToggleState(false, sendNotificationSync);
    REQUIRE_FALSE(enabled->getToggleState());
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    PrecisionSlider* noise = nullptr;
    for (int index = 0; index < editor.getNumChildComponents(); ++index) {
        auto* slider = dynamic_cast<PrecisionSlider*>(editor.getChildComponent(index));
        if (slider != nullptr && slider->getComponentID() == "guideEditor.noise") {
            noise = slider;
            break;
        }
    }
    REQUIRE(noise != nullptr);
    REQUIRE(noise->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(noise->getValue() == Catch::Approx(0.01));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    REQUIRE(noise->keyPressed(KeyPress(
            KeyPress::rightKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(noise->getValue() == Catch::Approx(0.011));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
}

TEST_CASE("Processing scope selector exposes two keyboard-selectable segments",
        "[cycle-v2][node-editor-host][processing-scope][accessibility]") {
    ScopedJuceInitialiser_GUI juce;
    ProcessingScopeSelector selector("testEditor");
    selector.setBounds(0, 0, 160, 28);
    selector.addToDesktop(0);
    selector.setVisible(true);
    String selected;
    selector.onChange = [&selected](const String& scope) { selected = scope; };

    auto* voice = dynamic_cast<Button*>(
            selector.findChildWithID("testEditor.processingScope.voice"));
    auto* global = dynamic_cast<Button*>(
            selector.findChildWithID("testEditor.processingScope.global"));
    REQUIRE(voice != nullptr);
    REQUIRE(global != nullptr);
    REQUIRE(voice->getBounds() == Rectangle<int>(0, 0, 80, 28));
    REQUIRE(global->getBounds() == Rectangle<int>(80, 0, 80, 28));
    REQUIRE(voice->getToggleState());
    REQUIRE_FALSE(global->getToggleState());

    voice->grabKeyboardFocus();
    REQUIRE(voice->hasKeyboardFocus(false));
    REQUIRE(static_cast<Component*>(voice)->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(global->hasKeyboardFocus(false));
    REQUIRE(static_cast<Component*>(global)->keyPressed(KeyPress(KeyPress::returnKey)));
    REQUIRE(selector.scope() == "global");
    REQUIRE(selected == "global");
    REQUIRE_FALSE(voice->getToggleState());
    REQUIRE(global->getToggleState());
}

TEST_CASE("Waveshaper editor preserves a square graph and semantic property rows",
        "[cycle-v2][node-editor-host][waveshaper][properties]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(NodeKind::Waveshaper);
    WaveshaperEditorComponent editor(widget);
    RecordingCurveDelegate delegate;
    Node node = GraphNodeFactory().createNode(NodeKind::Waveshaper, "waveshaper", {});
    for (auto& parameter : node.parameters) {
        if (parameter.id == "pre") {
            parameter.value = "0.75";
        } else if (parameter.id == "post") {
            parameter.value = "0.25";
        } else if (parameter.id == "aaFactor") {
            parameter.value = "4";
        }
    }

    editor.setDelegate(&delegate);
    editor.setBounds(0, 0, 766, 464);
    editor.setNode(node);

    const var state = editor.automationState();
    const Rectangle<float> panel = rectangleProperty(state, "panelBounds");
    const Rectangle<float> headerActionBounds = rectangleProperty(
            state,
            "headerActionBounds");
    REQUIRE(panel.getWidth() == Catch::Approx(panel.getHeight()));
    REQUIRE(panel.getWidth() >= 380.f);
    REQUIRE(headerActionBounds == embeddedEditorHeaderLayout(
            editor.getLocalBounds().toFloat(), true).enabled);
    const var preLayout = state.getProperty("preGainLayout", {});
    const var postLayout = state.getProperty("postGainLayout", {});
    const Rectangle<float> controlBounds = rectangleProperty(state, "controlBounds");
    const Rectangle<float> controlGroupBounds = rectangleProperty(state, "controlGroupBounds");
    REQUIRE(controlBounds.getWidth() == Catch::Approx(336.f));
    REQUIRE(controlGroupBounds.getX() - panel.getRight()
            == Catch::Approx(24.f));
    REQUIRE(controlGroupBounds.getCentreY()
            == Catch::Approx(controlBounds.reduced(12.f).getCentreY()).margin(1.f));
    REQUIRE(static_cast<bool>(preLayout.getProperty("compact", {})));
    REQUIRE(static_cast<bool>(postLayout.getProperty("compact", {})));
    REQUIRE(preLayout.getProperty("display", {}).toString() == "+23 dB");
    REQUIRE(postLayout.getProperty("display", {}).toString() == "-23 dB");
    REQUIRE(static_cast<int>(preLayout.getProperty("usableTrackWidth", {}))
            >= PropertyControlMetrics::minimumUsableTrackWidth);
    REQUIRE(state.getProperty("oversamplingDisplay", {}).toString() == "4x");
    const var processing = state.getProperty("processingScope", {});
    REQUIRE(processing.getProperty("scope", {}).toString() == "voice");
    REQUIRE(rectangleProperty(processing, "bounds").getWidth() >= 112.f);
    REQUIRE(rectangleProperty(processing, "bounds").getHeight() >= 28.f);
    REQUIRE(state.getProperty("processingGroup", {})
            .getProperty("label", {}).toString() == "Processing");
    REQUIRE(state.getProperty("gainGroup", {})
            .getProperty("label", {}).toString() == "Gain");
    const Rectangle<float> contextLabel = rectangleProperty(state, "contextLabelBounds");
    const Rectangle<float> contextSelector = rectangleProperty(state, "processingScopeBounds");
    const Rectangle<float> antialiasingLabel = rectangleProperty(
            state,
            "antialiasingLabelBounds");
    const Rectangle<float> oversamplingBounds = rectangleProperty(
            state,
            "oversamplingBounds");
    REQUIRE(contextLabel.getRight() == Catch::Approx(antialiasingLabel.getRight()));
    REQUIRE(contextSelector.getX() == Catch::Approx(oversamplingBounds.getX()));
    REQUIRE(contextSelector.getRight() == Catch::Approx(oversamplingBounds.getRight()));
    auto* oversampling = dynamic_cast<PropertySegmentedSelector*>(
            editor.findChildWithID("waveshaperEditor.oversampling"));
    auto* enabled = dynamic_cast<ToggleButton*>(
            editor.findChildWithID("waveshaperEditor.enabled"));
    auto* preGain = dynamic_cast<PrecisionSlider*>(
            editor.findChildWithID("waveshaperEditor.preGain"));
    REQUIRE(enabled != nullptr);
    REQUIRE(preGain != nullptr);
    REQUIRE(enabled->getBounds().toFloat() == headerActionBounds);
    REQUIRE(rectangleProperty(preLayout, "label").getY()
            == controlGroupBounds.getY()
                    + 2 * PropertyControlMetrics::groupLabelHeight
                    + 2 * PropertyControlMetrics::rowHeight
                    + PropertyControlMetrics::rowGap
                    + PropertyControlMetrics::sectionGap);
    REQUIRE(oversampling != nullptr);
    REQUIRE(oversampling->getWidth() == 176);
    REQUIRE(oversampling->selectedValue() == "4");
    for (int index = 1; index < 4; ++index) {
        REQUIRE(oversampling->optionBounds(index - 1).getRight()
                == Catch::Approx(oversampling->optionBounds(index).getX()));
    }
    auto* eightTimes = dynamic_cast<TextButton*>(oversampling->findChildWithID(
            "waveshaperEditor.oversampling.8x"));
    REQUIRE(eightTimes != nullptr);
    delegate.events.clear();
    eightTimes->onClick();
    REQUIRE(oversampling->selectedValue() == "8");
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
    auto* scopeSelector = dynamic_cast<ProcessingScopeSelector*>(
            editor.findChildWithID("waveshaperEditor.processingScope"));
    REQUIRE(scopeSelector != nullptr);
    auto* globalScope = dynamic_cast<Button*>(scopeSelector->findChildWithID(
            "waveshaperEditor.processingScope.global"));
    REQUIRE(globalScope != nullptr);
    delegate.events.clear();
    globalScope->onClick();
    REQUIRE(delegate.events == StringArray { "parameter" });
    REQUIRE(delegate.textParameterId == "processingScope");
    REQUIRE(delegate.textValue == "global");

    delegate.events.clear();
    auto* preGainValue = dynamic_cast<Label*>(
            editor.findChildWithID("waveshaperEditor.preGain.value"));
    REQUIRE(preGainValue != nullptr);
    preGainValue->setText("+12 dB", sendNotificationSync);
    REQUIRE(static_cast<double>(editor.automationState().getProperty("preGain", {}))
            == Catch::Approx(CycleDsp::waveshaperGainUnitValue(12.f)));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    preGainValue->setText("46 dB", sendNotificationSync);
    REQUIRE(static_cast<double>(editor.automationState().getProperty("preGain", {}))
            == Catch::Approx(CycleDsp::waveshaperGainUnitValue(12.f)));
    REQUIRE(delegate.events.isEmpty());
    REQUIRE_FALSE(static_cast<bool>(editor.automationState()
            .getProperty("preGainLayout", {})
            .getProperty("valid", {})));

    delegate.events.clear();
    const bool wasEnabled = enabled->getToggleState();
    enabled->setToggleState(!wasEnabled, sendNotificationSync);
    REQUIRE(enabled->getToggleState() != wasEnabled);
    REQUIRE(static_cast<bool>(editor.automationState().getProperty("enabled", {}))
            != wasEnabled);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
}

TEST_CASE("Impulse response editor exposes truthful precision properties",
        "[cycle-v2][node-editor-host][impulse-response][properties]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(NodeKind::ImpulseResponse);
    ImpulseResponseEditorComponent editor(widget);
    RecordingCurveDelegate delegate;
    Node ir = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});

    editor.setDelegate(&delegate);
    editor.setBounds(0, 0, 900, 430);
    editor.setNode(ir);

    const var narrowState = editor.automationState();
    editor.setBounds(0, 0, 1080, 430);
    const var state = editor.automationState();
    const var panel = state.getProperty("panelBounds", {});
    const var narrowPanel = narrowState.getProperty("panelBounds", {});
    const var controls = state.getProperty("controlBounds", {});
    const var narrowControls = narrowState.getProperty("controlBounds", {});
    REQUIRE(static_cast<double>(controls.getProperty("width", {}))
            == Catch::Approx(348.0));
    REQUIRE(static_cast<double>(controls.getProperty("width", {}))
            == Catch::Approx(static_cast<double>(
                    narrowControls.getProperty("width", {}))));
    REQUIRE(static_cast<double>(panel.getProperty("width", {}))
            == Catch::Approx(static_cast<double>(
                    narrowPanel.getProperty("width", {})) + 180.0));
    REQUIRE(static_cast<double>(panel.getProperty("height", {}))
            == Catch::Approx(static_cast<double>(
                    narrowPanel.getProperty("height", {}))));
    const Rectangle<float> headerActionBounds = rectangleProperty(
            state,
            "headerActionBounds");
    REQUIRE(static_cast<double>(panel.getProperty("width", {}))
            > static_cast<double>(panel.getProperty("height", {})));
    REQUIRE(headerActionBounds == embeddedEditorHeaderLayout(
            editor.getLocalBounds().toFloat(), true).enabled);
    const var processing = state.getProperty("processingScope", {});
    REQUIRE(processing.getProperty("scope", {}).toString() == "voice");
    REQUIRE(rectangleProperty(processing, "bounds").getWidth() >= 112.f);
    REQUIRE(rectangleProperty(processing, "bounds").getHeight() >= 28.f);
    REQUIRE(state.getProperty("processingGroup", {})
            .getProperty("label", {}).toString() == "PROCESSING");
    auto* scopeSelector = dynamic_cast<ProcessingScopeSelector*>(
            editor.findChildWithID("irEditor.processingScope"));
    REQUIRE(scopeSelector != nullptr);
    auto* globalScope = dynamic_cast<Button*>(scopeSelector->findChildWithID(
            "irEditor.processingScope.global"));
    REQUIRE(globalScope != nullptr);
    globalScope->onClick();
    REQUIRE(delegate.events == StringArray { "parameter" });
    REQUIRE(delegate.textParameterId == "processingScope");
    REQUIRE(delegate.textValue == "global");
    REQUIRE(state.getProperty("sizeLayout", {}).getProperty("display", {}).toString()
            == "1024 smp");
    REQUIRE(state.getProperty("postGainLayout", {}).getProperty("display", {}).toString()
            == "0 dB");
    REQUIRE(state.getProperty("highPassLayout", {}).getProperty("display", {}).toString()
            == "2.8 kHz");
    for (const Identifier property : {
            Identifier("sizeLayout"),
            Identifier("postGainLayout"),
            Identifier("highPassLayout") }) {
        REQUIRE(static_cast<bool>(state.getProperty(property, {})
                .getProperty("compact", {})));
        REQUIRE(static_cast<int>(state.getProperty(property, {})
                .getProperty("usableTrackWidth", {}))
                >= PropertyControlMetrics::minimumUsableTrackWidth);
    }
    const Array<var>* landmarks = state.getProperty("landmarks", {}).getArray();
    REQUIRE(landmarks != nullptr);
    const var panelState = state.getProperty("panelState", {});
    REQUIRE(static_cast<int>(panelState.getProperty("irSpectrumPointCount", {})) > 0);
    REQUIRE(static_cast<int>(panelState.getProperty("irFilteredImpulsePointCount", {}))
            == 1024);
    REQUIRE(panelState.getProperty("irBackdropRenderer", {}).toString() == "OpenGL");
    const auto modelledSource = prepareImpulseResponseSource(ir.parameters, ir.model);
    const auto modelledAnalysis = prepareImpulseResponseAnalysis(ir.parameters, ir.model);
    const Array<var>* waveformPoints = panelState.getProperty("waveformPoints", {}).getArray();
    REQUIRE(modelledSource.has_value());
    REQUIRE(modelledAnalysis.has_value());
    REQUIRE(waveformPoints != nullptr);
    REQUIRE_FALSE(waveformPoints->isEmpty());
    const auto editableSamples = irEditableSamplesAt(
            modelledSource->displayImpulse.size(), *waveformPoints);
    REQUIRE(irSampledCurveIdentityRootMeanSquare(
            modelledSource->displayImpulse, editableSamples) < 0.005);
    REQUIRE(significantTurningPointCount(modelledSource->displayImpulse, 0.002)
            <= significantTurningPointCount(editableSamples, 0.001));
    const float modelledFirstSample = panelState.getProperty(
            "irFilteredImpulseFirstSample", {});
    REQUIRE(modelledFirstSample
            == Catch::Approx(modelledAnalysis->filteredDisplayImpulse.front()));
    REQUIRE(static_cast<float>(panelState.getProperty("irAudioImpulseFirstSample", {}))
            == Catch::Approx(modelledAnalysis->filteredImpulse.front()));
    const Array<var>* majorGridLines = panelState
            .getProperty("verticalMajorGridLines", {})
            .getArray();
    REQUIRE(majorGridLines != nullptr);
    REQUIRE(landmarks->size() == majorGridLines->size());
    REQUIRE(landmarks->size() == 9);
    for (int index = 0; index < landmarks->size(); ++index) {
        REQUIRE((int) (*landmarks)[index].getProperty("sample", {})
                == index * 128);
    }
    const double panelX = panel.getProperty("x", {});
    const double panelWidth = panel.getProperty("width", {});
    const var zoom = panelState.getProperty("zoom", {});
    const double expectedZeroX = panelX + panelWidth
            * (CycleDsp::irDomainPadding
                    - static_cast<double>(zoom.getProperty("x", {})))
            / static_cast<double>(zoom.getProperty("w", {}));
    REQUIRE(static_cast<double>(landmarks->getFirst().getProperty("x", {}))
            == Catch::Approx(expectedZeroX));
    for (int index = 0; index < landmarks->size(); ++index) {
        REQUIRE(static_cast<double>((*landmarks)[index].getProperty("x", {}))
                == Catch::Approx(
                        panelX
                        + static_cast<double>((*majorGridLines)[index]
                                .getProperty("panelX", {}))));
    }
    const var firstLandmark = landmarks->getFirst();
    const var lastLandmark = landmarks->getLast();
    REQUIRE(static_cast<double>(lastLandmark.getProperty("x", {}))
            == Catch::Approx(panelX + panelWidth));
    REQUIRE(static_cast<double>(firstLandmark.getProperty("labelX", {}))
                    + static_cast<double>(firstLandmark.getProperty("labelWidth", {})) * 0.5
            == Catch::Approx(static_cast<double>(firstLandmark.getProperty("x", {}))));
    REQUIRE(static_cast<double>(lastLandmark.getProperty("labelX", {}))
                    + static_cast<double>(lastLandmark.getProperty("labelWidth", {}))
            <= panelX + panelWidth + 0.001);
    REQUIRE((bool) state.getProperty("resourceActionsAvailable", {}));
    REQUIRE_FALSE((bool) state.getProperty("resourceBound", {}));
    REQUIRE(state.getProperty("resourceSectionLabel", {}).toString() == "IR sample");
    REQUIRE(state.getProperty("responseGroup", {})
            .getProperty("label", {}).toString() == "Response");
    REQUIRE(state.getProperty("resourceGroup", {})
            .getProperty("label", {}).toString() == "IR sample");
    REQUIRE_FALSE((bool) state.getProperty("resourceSublabelVisible", {}));

    int textButtonCount = 0;
    for (int index = 0; index < editor.getNumChildComponents(); ++index) {
        textButtonCount += dynamic_cast<TextButton*>(editor.getChildComponent(index)) != nullptr
                ? 1 : 0;
    }
    REQUIRE(textButtonCount == 3);
    auto* loadAudio = dynamic_cast<TextButton*>(editor.findChildWithID("irEditor.loadAudio"));
    auto* modelAudio = dynamic_cast<TextButton*>(editor.findChildWithID("irEditor.modelAudio"));
    auto* unloadAudio = dynamic_cast<TextButton*>(editor.findChildWithID("irEditor.unloadAudio"));
    REQUIRE(loadAudio != nullptr);
    REQUIRE(modelAudio != nullptr);
    REQUIRE(unloadAudio != nullptr);
    REQUIRE(loadAudio->getButtonText() == "Load");
    REQUIRE(modelAudio->getButtonText() == "Model");
    REQUIRE(unloadAudio->getButtonText() == "Unload");
    REQUIRE(loadAudio->getWantsKeyboardFocus());

    AudioSampleResource resource { "direct", "Direct.wav", 48000.0, {} };
    resource.samples.resize(1024);
    resource.samples.front() = 1.f;
    widget.setImpulseResponseAudioResource(&resource);
    const var directPanelState = editor.automationState()
            .getProperty("panelState", {});
    const auto directAnalysis = prepareImpulseResponseAnalysis(
            ir.parameters, ir.model, &resource);
    REQUIRE(directAnalysis.has_value());
    REQUIRE(static_cast<float>(directPanelState.getProperty(
                    "irFilteredImpulseFirstSample", {}))
            == Catch::Approx(directAnalysis->filteredImpulse.front()));
    REQUIRE(static_cast<float>(directPanelState.getProperty(
                    "irFilteredImpulseFirstSample", {}))
            != Catch::Approx(modelledFirstSample));
    REQUIRE(modelAudio->getWantsKeyboardFocus());
    REQUIRE_FALSE(unloadAudio->isEnabled());
    REQUIRE(loadAudio->getHeight() == 24);
    REQUIRE(modelAudio->getHeight() == loadAudio->getHeight());
    REQUIRE(unloadAudio->getHeight() == loadAudio->getHeight());
    REQUIRE(loadAudio->getY() == modelAudio->getY());
    REQUIRE(modelAudio->getY() == unloadAudio->getY());
    REQUIRE(loadAudio->getWidth() < 100);

    auto* enabled = dynamic_cast<ToggleButton*>(editor.findChildWithID("irEditor.enabled"));
    REQUIRE(enabled != nullptr);
    REQUIRE(enabled->isVisible());
    REQUIRE(enabled->getLocalBounds().contains(15, 15));
    REQUIRE(enabled->getBounds().toFloat() == headerActionBounds);

    auto* sizeSlider = dynamic_cast<PrecisionSlider*>(editor.findChildWithID("irEditor.size"));
    REQUIRE(sizeSlider != nullptr);
    const Rectangle<float> sizeLabel = rectangleProperty(
            state.getProperty("sizeLayout", {}), "label");
    const Rectangle<float> sizeTrack = rectangleProperty(
            state.getProperty("sizeLayout", {}), "track");
    REQUIRE(sizeLabel.getY()
            == rectangleProperty(state, "controlBounds").toNearestInt()
                    .reduced(12, 12).getY()
                    + 2 * PropertyControlMetrics::groupLabelHeight
                    + 28
                    + PropertyControlMetrics::sectionGap);
    REQUIRE(sizeTrack.getY() - sizeLabel.getBottom() <= 10.f);

    delegate.events.clear();
    const bool wasEnabled = enabled->getToggleState();
    enabled->setToggleState(!wasEnabled, sendNotificationSync);
    REQUIRE(enabled->getToggleState() != wasEnabled);
    REQUIRE(static_cast<bool>(editor.automationState().getProperty("enabled", {}))
            != wasEnabled);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    auto* sizeValue = dynamic_cast<Label*>(editor.findChildWithID("irEditor.size.value"));
    REQUIRE(sizeValue != nullptr);
    sizeValue->setText("4096 samples", sendNotificationSync);
    REQUIRE(CycleDsp::irImpulseLength(static_cast<double>(
            editor.automationState().getProperty("size", {}))) == 4096);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    sizeValue->setText("3000 samples", sendNotificationSync);
    REQUIRE(CycleDsp::irImpulseLength(static_cast<double>(
            editor.automationState().getProperty("size", {}))) == 4096);
    REQUIRE_FALSE((bool) editor.automationState()
            .getProperty("sizeLayout", {})
            .getProperty("valid", {}));
    REQUIRE(delegate.events.isEmpty());

    auto* highPass = dynamic_cast<PrecisionSlider*>(
            editor.findChildWithID("irEditor.highPass"));
    REQUIRE(highPass != nullptr);
    const double oneKilohertzValue = CycleDsp::irPrefilterValueForFrequency(
            1000.f, 44100.0);
    const double minimumPosition = highPass->getPositionOfValue(highPass->getMinimum());
    const double maximumPosition = highPass->getPositionOfValue(highPass->getMaximum());
    REQUIRE(highPass->getPositionOfValue(oneKilohertzValue)
            == Catch::Approx((minimumPosition + maximumPosition) * 0.5).margin(1.0));

    const var beforeHighPassPanel = editor.automationState().getProperty("panelState", {});
    const int64 sourceRevision = beforeHighPassPanel.getProperty("irSourceRevision", {});
    const int64 analysisRevision = beforeHighPassPanel.getProperty("irAnalysisRevision", {});
    const float filteredSample = beforeHighPassPanel.getProperty(
            "irFilteredImpulseFirstSample", {});
    const float cutoffBefore = CycleDsp::irPrefilterFrequency(
            highPass->getValue(),
            44100.0);
    REQUIRE(highPass->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(CycleDsp::irPrefilterFrequency(highPass->getValue(), 44100.0)
            == Catch::Approx(cutoffBefore + 100.f).margin(0.1f));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
    const var afterHighPassPanel = editor.automationState().getProperty("panelState", {});
    REQUIRE(static_cast<int64>(afterHighPassPanel.getProperty("irSourceRevision", {}))
            == sourceRevision);
    REQUIRE(static_cast<int64>(afterHighPassPanel.getProperty("irAnalysisRevision", {}))
            > analysisRevision);
    REQUIRE(static_cast<float>(afterHighPassPanel.getProperty(
                    "irFilteredImpulseFirstSample", {}))
            != Catch::Approx(filteredSample));

    delegate.events.clear();
    auto* highPassValue = dynamic_cast<Label*>(
            editor.findChildWithID("irEditor.highPass.value"));
    REQUIRE(highPassValue != nullptr);
    highPassValue->setText("880 Hz", sendNotificationSync);
    REQUIRE(CycleDsp::irPrefilterFrequency(highPass->getValue(), 44100.0)
            == Catch::Approx(880.f).margin(0.1f));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.events.clear();
    auto* postGain = dynamic_cast<PrecisionSlider*>(
            editor.findChildWithID("irEditor.postGain"));
    REQUIRE(postGain != nullptr);
    const var beforePostPanel = editor.automationState().getProperty("panelState", {});
    const int64 sourceRevisionBeforePost = beforePostPanel.getProperty(
            "irSourceRevision", {});
    const int64 analysisRevisionBeforePost = beforePostPanel.getProperty(
            "irAnalysisRevision", {});
    const float displayedSampleBeforePost = beforePostPanel.getProperty(
            "irDisplayedImpulseFirstSample", {});
    REQUIRE(postGain->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(CycleDsp::irPostGainDecibels(postGain->getValue())
            == Catch::Approx(1.f).margin(0.001f));
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });
    const var afterPostPanel = editor.automationState().getProperty("panelState", {});
    REQUIRE(static_cast<int64>(afterPostPanel.getProperty("irSourceRevision", {}))
            == sourceRevisionBeforePost);
    REQUIRE(static_cast<int64>(afterPostPanel.getProperty("irAnalysisRevision", {}))
            == analysisRevisionBeforePost);
    REQUIRE(static_cast<float>(afterPostPanel.getProperty(
                    "irDisplayedImpulseFirstSample", {}))
            != Catch::Approx(displayedSampleBeforePost));

    delegate.events.clear();
    const int64 sourceRevisionBeforeSize = afterPostPanel.getProperty(
            "irSourceRevision", {});
    const int64 analysisRevisionBeforeSize = afterPostPanel.getProperty(
            "irAnalysisRevision", {});
    REQUIRE(sizeSlider->keyPressed(KeyPress(KeyPress::leftKey)));
    const var afterSizePanel = editor.automationState().getProperty("panelState", {});
    REQUIRE(static_cast<int64>(afterSizePanel.getProperty("irSourceRevision", {}))
            == sourceRevisionBeforeSize);
    REQUIRE(static_cast<int64>(afterSizePanel.getProperty("irAnalysisRevision", {}))
            == analysisRevisionBeforeSize);
    REQUIRE(delegate.events == StringArray { "begin", "repaint", "publish", "commit" });

    delegate.resource = NodeAudioResourceSummary { "room.wav", "direct", 101 };
    editor.setNode(ir);
    REQUIRE((bool) editor.automationState().getProperty("resourceBound", {}));
    REQUIRE(editor.automationState().getProperty("resourceMode", {}).toString() == "direct");
    REQUIRE(unloadAudio->isEnabled());
    REQUIRE(unloadAudio->getWantsKeyboardFocus());

    delegate.resourceRemovalSucceeds = false;
    unloadAudio->onClick();
    REQUIRE(delegate.status == "The embedded audio could not be unloaded.");
    REQUIRE(delegate.resourceRemovals == 0);

    delegate.resourceRemovalSucceeds = true;
    unloadAudio->onClick();
    REQUIRE(delegate.resourceRemovals == 1);
    REQUIRE(delegate.status.isEmpty());
    REQUIRE_FALSE((bool) editor.automationState().getProperty("resourceBound", {}));
}

TEST_CASE("IR attack zoom changes presentation without publishing the graph",
        "[cycle-v2][node-editor-host][impulse-response][zoom]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    CurveEditorWidget widget(NodeKind::ImpulseResponse);
    ImpulseResponseEditorComponent editor(widget);
    RecordingCurveDelegate delegate;
    const Node ir = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});

    editor.setDelegate(&delegate);
    editor.setBounds(0, 0, 1080, 430);
    editor.setNode(ir);

    auto* zoomAttack = dynamic_cast<Button*>(
            editor.findChildWithID("irEditor.zoomAttack"));
    auto* zoomFull = dynamic_cast<Button*>(
            editor.findChildWithID("irEditor.zoomFull"));
    REQUIRE(zoomAttack != nullptr);
    REQUIRE(zoomFull != nullptr);
    REQUIRE(zoomAttack->isVisible());
    REQUIRE(zoomFull->isVisible());
    REQUIRE(zoomAttack->getWantsKeyboardFocus());
    REQUIRE(zoomFull->getWantsKeyboardFocus());
    REQUIRE(zoomAttack->getTooltip() == "Zoom to the impulse attack");
    REQUIRE(zoomFull->getTooltip() == "Show the full impulse response");

    const var fullState = editor.automationState();
    const var fullZoom = fullState
            .getProperty("panelState", {})
            .getProperty("zoom", {});
    const Rectangle<float> panel = rectangleProperty(fullState, "panelBounds");
    const double fullX = fullZoom.getProperty("x", {});
    const double fullY = fullZoom.getProperty("y", {});
    const double fullWidth = fullZoom.getProperty("w", {});
    const double fullHeight = fullZoom.getProperty("h", {});

    delegate.events.clear();
    zoomAttack->onClick();
    const var attackState = editor.automationState();
    const var attackZoom = attackState
            .getProperty("panelState", {})
            .getProperty("zoom", {});
    REQUIRE(static_cast<double>(attackZoom.getProperty("x", {}))
            == Catch::Approx(CycleDsp::irDomainPadding));
    REQUIRE(static_cast<double>(attackZoom.getProperty("w", {}))
            == Catch::Approx(fullWidth * 0.2));
    REQUIRE(static_cast<double>(attackZoom.getProperty("y", {}))
            == Catch::Approx(fullY));
    REQUIRE(static_cast<double>(attackZoom.getProperty("h", {}))
            == Catch::Approx(fullHeight));
    REQUIRE(delegate.events == StringArray { "repaint" });

    const Array<var>* attackLandmarks = attackState.getProperty("landmarks", {}).getArray();
    REQUIRE(attackLandmarks != nullptr);
    REQUIRE(attackLandmarks->size() == 2);
    REQUIRE((int) attackLandmarks->getFirst().getProperty("sample", {}) == 0);
    REQUIRE((int) attackLandmarks->getLast().getProperty("sample", {}) == 128);
    for (const var& landmark : *attackLandmarks) {
        const double x = landmark.getProperty("x", {});
        REQUIRE(x >= panel.getX());
        REQUIRE(x <= panel.getRight());
    }
    REQUIRE(rectangleProperty(attackState, "zoomAttackBounds").getWidth() == 24.f);
    REQUIRE(rectangleProperty(attackState, "zoomFullBounds").getWidth() == 24.f);
    REQUIRE(panel.contains(rectangleProperty(attackState, "zoomAttackBounds")));
    REQUIRE(panel.contains(rectangleProperty(attackState, "zoomFullBounds")));

    delegate.events.clear();
    zoomFull->onClick();
    const var restoredZoom = editor.automationState()
            .getProperty("panelState", {})
            .getProperty("zoom", {});
    REQUIRE(static_cast<double>(restoredZoom.getProperty("x", {}))
            == Catch::Approx(fullX));
    REQUIRE(static_cast<double>(restoredZoom.getProperty("w", {}))
            == Catch::Approx(fullWidth));
    REQUIRE(static_cast<double>(restoredZoom.getProperty("y", {}))
            == Catch::Approx(fullY));
    REQUIRE(static_cast<double>(restoredZoom.getProperty("h", {}))
            == Catch::Approx(fullHeight));
    REQUIRE(delegate.events == StringArray { "repaint" });
}

TEST_CASE("Guide editor presents heatmap state and clears through its semantic action",
        "[cycle-v2][node-editor-host][guides][heatmap]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Image image(Image::RGB, 2, 2, false);
    image.setPixelAt(0, 0, Colours::black);
    image.setPixelAt(1, 0, Colours::white);
    image.setPixelAt(0, 1, Colours::white);
    image.setPixelAt(1, 1, Colours::black);
    MemoryOutputStream output;
    PNGImageFormat format;
    REQUIRE(format.writeImageToStream(image, output));
    String error;
    const auto heatmap = GuideHeatmapAsset::decode(
            output.getMemoryBlock(), "checker.png", error);
    REQUIRE(heatmap != nullptr);

    CurveEditorWidget widget(true);
    GuideCurveEditorComponent editor(widget);
    GuideCurveResource guide;
    guide.id = "guide1";
    guide.model = createDefaultGuideCurveModel();
    bool cleared = false;
    editor.setHeatmapActions(
            {},
            [&](const String& guideId, uint64_t revision) {
                cleared = guideId == guide.id && revision == guide.revision;
                return true;
            });
    editor.setGuideResource(guide, heatmap);

    const var editorState = editor.automationState();
    REQUIRE((bool) editorState.getProperty("heatmapActive", {}));
    REQUIRE(editorState.getProperty("heatmapFilename", {}).toString() == "checker.png");
    REQUIRE(editorState.getProperty("heatmapSectionLabel", {}).toString() == "Guide image");
    REQUIRE_FALSE((bool) editorState.getProperty("heatmapSublabelVisible", {}));
    const var panelState = widget.automationState();
    REQUIRE((bool) panelState.getProperty("heatmapActive", {}));
    REQUIRE((int) panelState.getProperty("heatmapOutputPointCount", {}) == 512);
    const int initialTextureRevision = panelState.getProperty(
            "heatmapTextureRevision", {});
    const double initialOutput = panelState.getProperty("heatmapOutputStart", {});
    REQUIRE(widget.setSelectedVertexParameter("vertex.amp", 0.1f));
    REQUIRE(widget.modelPublication() != nullptr);
    const var updatedPanelState = widget.automationState();
    REQUIRE((double) updatedPanelState.getProperty("heatmapOutputStart", {})
            != Catch::Approx(initialOutput).margin(0.001));
    REQUIRE((int) updatedPanelState.getProperty("heatmapTextureRevision", {})
            == initialTextureRevision);

    auto* loadButton = dynamic_cast<TextButton*>(
            editor.findChildWithID("guideEditor.loadImage"));
    auto* clearButton = dynamic_cast<TextButton*>(
            editor.findChildWithID("guideEditor.clearImage"));
    REQUIRE(loadButton != nullptr);
    REQUIRE(clearButton != nullptr);
    REQUIRE(loadButton->getButtonText() == "Replace");
    REQUIRE(clearButton->isEnabled());
    REQUIRE(loadButton->getY() == clearButton->getY());
    REQUIRE(loadButton->getHeight() == 24);
    clearButton->onClick();
    REQUIRE(cleared);
    REQUIRE_FALSE(clearButton->isEnabled());

    editor.setGuideResource(guide, nullptr);
    REQUIRE_FALSE((bool) widget.automationState().getProperty("heatmapActive", {}));
}

TEST_CASE("Waveshaper point drag retains the hovered vertex identity",
        "[cycle-v2][node-editor-host][waveshaper][interaction][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Node node = GraphNodeFactory().createNode(NodeKind::Waveshaper, "waveshaper", {});
    CurveEditorWidget widget(NodeKind::Waveshaper);
    Component* panel = widget.prepareExpandedPanelComponent(
            node, Rectangle<float>(0.f, 0.f, 436.f, 436.f));
    REQUIRE(panel != nullptr);
    panel->setBounds(0, 0, 436, 436);
    panel->addToDesktop(ComponentPeer::windowIsTemporary);

    const auto pointFor = [&](float x, float y) {
        const var zoom = widget.automationState().getProperty("zoom", {});
        const float zoomX = zoom.getProperty("x", {});
        const float zoomY = zoom.getProperty("y", {});
        const float zoomWidth = zoom.getProperty("w", {});
        const float zoomHeight = zoom.getProperty("h", {});
        return Point<float>(
                panel->getWidth() * (x - zoomX) / zoomWidth,
                panel->getHeight() * (1.f - (y - zoomY) / zoomHeight));
    };

    const Point<float> insertion = pointFor(0.5f, 0.32f);
    panel->mouseDoubleClick(curvePanelMouseEvent(
            *panel,
            insertion,
            ModifierKeys::leftButtonModifier,
            insertion,
            false,
            2));
    NodeModelStatePtr insertedPublication = widget.prepareModelPublication(
            node.model->revision());
    REQUIRE(insertedPublication != nullptr);
    const auto insertedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
            insertedPublication);
    REQUIRE(insertedModel != nullptr);
    REQUIRE(insertedModel->flatCurve() != nullptr);
    REQUIRE(insertedModel->flatCurve()->selectedVertexId().has_value());
    const CurveVertexId insertedId = *insertedModel->flatCurve()->selectedVertexId();
    const auto insertedVertices = insertedModel->flatCurve()->getVertices();
    const auto inserted = std::find_if(
            insertedVertices.begin(),
            insertedVertices.end(),
            [&](const auto& vertex) { return vertex.id == insertedId; });
    REQUIRE(inserted != insertedVertices.end());
    const FlatCurveVertex insertedVertex = *inserted;

    node.model = std::move(insertedPublication);
    auto* editorState = new DynamicObject();
    editorState->setProperty("selectedVertexId", (int64) insertedId);
    node.editorState = var(editorState);
    widget.syncFromNode(node);

    const Point<float> source = pointFor(insertedVertex.x, insertedVertex.y);
    panel->mouseMove(curvePanelMouseEvent(*panel, source, {}, source, false));
    const var hovered = widget.automationState().getProperty("currentVertex", {});
    REQUIRE(static_cast<double>(hovered.getProperty("x", {}))
            == Catch::Approx(insertedVertex.x));
    REQUIRE(static_cast<double>(hovered.getProperty("y", {}))
            == Catch::Approx(insertedVertex.y));

    panel->mouseDown(curvePanelMouseEvent(
            *panel,
            source,
            ModifierKeys::leftButtonModifier,
            source,
            false));
    const Point<float> destination = pointFor(0.66f, 0.48f);
    for (int step = 1; step <= 6; ++step) {
        const float amount = (float) step / 6.f;
        const Point<float> position = source + (destination - source) * amount;
        panel->mouseDrag(curvePanelMouseEvent(
                *panel,
                position,
                ModifierKeys::leftButtonModifier,
                source,
                true));
    }
    panel->mouseUp(curvePanelMouseEvent(*panel, destination, {}, source, true));

    const auto movedModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
            widget.prepareModelPublication(node.model->revision()));
    REQUIRE(movedModel != nullptr);
    REQUIRE(movedModel->flatCurve() != nullptr);
    const auto movedVertices = movedModel->flatCurve()->getVertices();
    const auto moved = std::find_if(
            movedVertices.begin(),
            movedVertices.end(),
            [&](const auto& vertex) { return vertex.id == insertedId; });
    REQUIRE(moved != movedVertices.end());
    REQUIRE(std::abs(moved->x - insertedVertex.x) > 0.02f);
    for (const auto& before : insertedVertices) {
        if (before.id == insertedId) {
            continue;
        }
        const auto after = std::find_if(
                movedVertices.begin(),
                movedVertices.end(),
                [&](const auto& vertex) { return vertex.id == before.id; });
        REQUIRE(after != movedVertices.end());
        REQUIRE(*after == before);
    }
}

TEST_CASE("Hosted Waveshaper movements publish stable point previews before release",
        "[cycle-v2][node-editor-host][waveshaper][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Node node = GraphNodeFactory().createNode(NodeKind::Waveshaper, "waveshaper", {});
    const auto original = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    REQUIRE(original != nullptr);
    REQUIRE(original->flatCurve() != nullptr);
    const FlatCurveVertex point = original->flatCurve()->getVertices()[1];

    CurveEditorWidget widget(NodeKind::Waveshaper);
    WaveshaperEditorComponent editor(widget);
    RecordingCurveDelegate delegate;
    editor.setDelegate(&delegate);
    editor.setBounds(0, 0, 766, 464);
    editor.setNode(node);
    Component* panel = widget.getExpandedPanelComponentIfCreated();
    REQUIRE(panel != nullptr);
    panel->addToDesktop(ComponentPeer::windowIsTemporary);

    const var zoom = widget.automationState().getProperty("zoom", {});
    const auto panelPoint = [&](float x, float y) {
        return Point<float>(
                panel->getWidth() * (x - (float) zoom.getProperty("x", {}))
                        / (float) zoom.getProperty("w", {}),
                panel->getHeight() * (1.f - (y - (float) zoom.getProperty("y", {}))
                        / (float) zoom.getProperty("h", {})));
    };
    const Point<float> source = panelPoint(point.x, point.y);
    panel->mouseMove(curvePanelMouseEvent(*panel, source, {}, source, false));
    panel->mouseDown(curvePanelMouseEvent(
            *panel, source, ModifierKeys::leftButtonModifier, source, false));

    InteractionComplexityDiagnostics::reset();
    size_t movementCount {};
    for (const Point<float> destination : {
            panelPoint(0.18f, 0.25f), panelPoint(0.22f, 0.33f) }) {
        panel->mouseDrag(curvePanelMouseEvent(
                *panel,
                destination,
                ModifierKeys::leftButtonModifier,
                source,
                true));
        ++movementCount;
        REQUIRE(delegate.publications.size() == movementCount);
        const auto preview = std::dynamic_pointer_cast<const CurveNodeModelState>(
                delegate.publications.back());
        REQUIRE(preview != nullptr);
        REQUIRE(preview->pointPreview().has_value());
        REQUIRE(preview->pointPreview()->id == point.id);
        REQUIRE(preview->flatCurve() == original->flatCurve());
    }
    REQUIRE(delegate.publications.size() == 2);
    const auto movementCounts = InteractionComplexityDiagnostics::counts();
    REQUIRE(movementCounts.meshCopies == 0);
    REQUIRE(movementCounts.graphCopies == 0);
    REQUIRE(movementCounts.modelSerializations == 0);
    REQUIRE(original->flatCurve()->getVertices()[1] == point);

    const Point<float> destination = panelPoint(0.22f, 0.33f);
    panel->mouseUp(curvePanelMouseEvent(*panel, destination, {}, source, true));
    REQUIRE(delegate.publications.size() == 3);
    const auto committed = std::dynamic_pointer_cast<const CurveNodeModelState>(
            delegate.publications.back());
    REQUIRE(committed != nullptr);
    REQUIRE_FALSE(committed->pointPreview().has_value());
    REQUIRE(committed->flatCurve()->getVertices()[1].id == point.id);
    REQUIRE(committed->flatCurve()->getVertices()[1].y != point.y);

    delegate.allowCancel = true;
    const FlatCurveVertex committedPoint = committed->flatCurve()->getVertices()[1];
    const Point<float> cancelSource = panelPoint(committedPoint.x, committedPoint.y);
    panel->mouseMove(curvePanelMouseEvent(
            *panel, cancelSource, {}, cancelSource, false));
    panel->mouseDown(curvePanelMouseEvent(
            *panel,
            cancelSource,
            ModifierKeys::leftButtonModifier,
            cancelSource,
            false));
    const Point<float> cancelledDestination = panelPoint(0.28f, 0.42f);
    panel->mouseDrag(curvePanelMouseEvent(
            *panel,
            cancelledDestination,
            ModifierKeys::leftButtonModifier,
            cancelSource,
            true));
    REQUIRE(delegate.publications.size() == 4);
    REQUIRE(panel->keyPressed(KeyPress(KeyPress::escapeKey)));
    panel->mouseUp(curvePanelMouseEvent(
            *panel, cancelledDestination, {}, cancelSource, true));
    REQUIRE(delegate.publications.size() == 4);
    REQUIRE(delegate.events.contains("cancel"));
    REQUIRE(widget.pointPreview().has_value());
    REQUIRE(widget.pointPreview()->id == committedPoint.id);
    REQUIRE(widget.pointPreview()->x == Catch::Approx(committedPoint.x));
    REQUIRE(widget.pointPreview()->y == Catch::Approx(committedPoint.y));
}

TEST_CASE("Flat curve editors highlight the closest vertex across the panel",
        "[cycle-v2][node-editor-host][curve][hover][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;

    const auto verifyHoverHighlight = [](NodeKind kind, const String& nodeId) {
        Node node = GraphNodeFactory().createNode(kind, nodeId, {});
        CurveEditorWidget widget(kind);
        Component* panel = widget.prepareExpandedPanelComponent(
                node, Rectangle<float>(0.f, 0.f, 436.f, 436.f));
        REQUIRE(panel != nullptr);
        panel->setBounds(0, 0, 436, 436);
        panel->addToDesktop(ComponentPeer::windowIsTemporary);

        const var zoom = widget.automationState().getProperty("zoom", {});
        const float zoomX = zoom.getProperty("x", {});
        const float zoomY = zoom.getProperty("y", {});
        const float zoomWidth = zoom.getProperty("w", {});
        const float zoomHeight = zoom.getProperty("h", {});
        const auto panelPoint = [&](float x, float y) {
            return Point<float>(
                    panel->getWidth() * (x - zoomX) / zoomWidth,
                    panel->getHeight() * (1.f - (y - zoomY) / zoomHeight));
        };
        const Point<float> vertexPosition = panelPoint(0.5f, 0.35f);

        panel->mouseDoubleClick(curvePanelMouseEvent(
                *panel,
                vertexPosition,
                ModifierKeys::leftButtonModifier,
                vertexPosition,
                false,
                2));
        const Point<float> distantPointer = panelPoint(0.5f, 0.85f);
        panel->mouseMove(curvePanelMouseEvent(
                *panel, distantPointer, {}, distantPointer, false));
        const var hoverState = widget.automationState();
        REQUIRE((bool) hoverState.getProperty(
                "hoverHighlightVisible", false));
        const var highlightedVertex = hoverState.getProperty("currentVertex", {});
        const Point<float> highlightedPosition = panelPoint(
                highlightedVertex.getProperty("x", {}),
                highlightedVertex.getProperty("y", {}));
        REQUIRE(highlightedPosition.getDistanceFrom(distantPointer) > 15.f);

        panel->mouseExit(curvePanelMouseEvent(
                *panel, distantPointer, {}, distantPointer, false));
        REQUIRE_FALSE((bool) widget.automationState().getProperty(
                "hoverHighlightVisible", true));
    };

    verifyHoverHighlight(NodeKind::Waveshaper, "waveshaper");
    verifyHoverHighlight(NodeKind::ImpulseResponse, "impulse-response");
}

TEST_CASE("Selected flat curve state binds before its panel host exists",
          "[cycle-v2][node-editor-host][presets][selection]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    const NodeGraph stengah = GraphSerializer().fromJsonString(
            File(CYCLE_V2_SOURCE_DIR)
                    .getChildFile("content")
                    .getChildFile("presets")
                    .getChildFile("stengah.cyclegraph")
                    .loadFileAsString());
    const Node* waveshaper = stengah.findNode("waveshaper");
    REQUIRE(waveshaper != nullptr);
    REQUIRE((int64) waveshaper->editorState.getProperty("selectedVertexId", {}) > 0);

    CurveEditorWidget widget(NodeKind::Waveshaper);
    widget.syncFromNode(*waveshaper);

    REQUIRE_FALSE(widget.selectedVertexParameters().empty());
    REQUIRE(widget.prepareExpandedPanelComponent(
            *waveshaper,
            Rectangle<float>(0.f, 0.f, 640.f, 400.f)) != nullptr);
    REQUIRE_FALSE(widget.selectedVertexParameters().empty());
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Envelope purpose selector publishes bipolar pitch presentation",
        "[cycle-v2][node-editor-host][envelope][purpose]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    GraphNodeFactory factory;
    GraphEditor graphEditor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "dimensionCount", "Curve morphing", "2").succeeded());
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "purpose", "Purpose", "pitch").succeeded());
    EnvelopeNodeModel envelopeModel;
    for (VertCube* cube : envelopeModel.getMesh().getCubes()) {
        for (int index = 0; index < (int) VertCube::numVerts; ++index) {
            cube->getVertex(index)->values[Vertex::Amp] = 0.5f;
        }
    }
    REQUIRE(envelopeModel.synchronizeFromMesh(nullptr));
    REQUIRE(graph.replaceNodeModel(
            "env",
            CurveNodeModelState::copyOf(
                    envelopeModel, 0.5f, 0.5f, envelopeModel.revision() + 1)));

    CurveEditorWidget widget(NodeKind::Envelope);
    widget.syncFromNode(*graph.findNode("env"));
    REQUIRE(widget.getExpandedPanelComponentIfCreated() == nullptr);
    auto panelState = widget.automationState();
    REQUIRE(widget.getExpandedPanelComponentIfCreated() == nullptr);
    REQUIRE_FALSE((bool) panelState.getProperty("hasDisplayCoordinates", {}));
    REQUIRE((bool) panelState.getProperty("bipolar", {}));
    REQUIRE(static_cast<double>(panelState.getProperty("verticalZoomHeight", {})) < 0.1);

    auto editor = createCurveNodeEditor(NodeKind::Envelope, widget);
    RecordingCurveDelegate delegate;
    editor->setDelegate(&delegate);
    editor->setBounds(0, 0, 840, 684);
    editor->setNode(*graph.findNode("env"));
    const var state = editor->automationState();
    panelState = widget.automationState();
    REQUIRE((bool) panelState.getProperty("hasDisplayCoordinates", {}));
    REQUIRE(state.getProperty("purpose", {}).toString() == "Pitch");
    REQUIRE(state.getProperty("polarity", {}).toString() == "bipolar");
    const auto purposeBounds = rectangleProperty(state, "purposeBounds");
    const auto purposeLabelBounds = rectangleProperty(state, "purposeLabelBounds");
    const auto blueMorphBounds = rectangleProperty(state, "blueMorphBounds");
    REQUIRE_FALSE(state.hasProperty("actionBarBounds"));
    REQUIRE_FALSE(state.hasProperty("actionRowBounds"));
    const auto controlBounds = rectangleProperty(state, "controlBounds");
    const auto panelBounds = rectangleProperty(state, "panelBounds");
    REQUIRE(purposeBounds.getWidth() > 0.f);
    REQUIRE(blueMorphBounds.getWidth() > 0.f);
    REQUIRE(controlBounds.getWidth() > 0.f);
    REQUIRE(blueMorphBounds.getBottom() < panelBounds.getY());
    panelState = widget.automationState();
    REQUIRE((bool) panelState.getProperty("bipolar", {}));
    REQUIRE(static_cast<double>(panelState.getProperty("verticalZoomHeight", {})) < 0.1);
    EnvelopePurposeSelector* modeSelector = nullptr;
    EnvelopeAxisScaleSelector* axisScaleSelector = nullptr;
    Button* pitchMode = nullptr;
    Button* scratchMode = nullptr;
    Button* loopMarker = nullptr;
    Button* sustainMarker = nullptr;
    Button* fitVertical = nullptr;
    Button* fullVertical = nullptr;
    EffectEnableButton* enabled = nullptr;
    for (int index = 0; index < editor->getNumChildComponents(); ++index) {
        if (auto* selector = dynamic_cast<EnvelopePurposeSelector*>(editor->getChildComponent(index))) {
            modeSelector = selector;
            for (int option = 0; option < selector->getNumChildComponents(); ++option) {
                auto* button = dynamic_cast<Button*>(selector->getChildComponent(option));
                if (button != nullptr && button->getName() == "Pitch envelope mode") {
                    pitchMode = button;
                } else if (button != nullptr && button->getName() == "Scratch envelope mode") {
                    scratchMode = button;
                }
            }
        } else if (auto* selector = dynamic_cast<EnvelopeAxisScaleSelector*>(
                           editor->getChildComponent(index))) {
            axisScaleSelector = selector;
        } else if (auto* button = dynamic_cast<Button*>(editor->getChildComponent(index))) {
            if (auto* enableButton = dynamic_cast<EffectEnableButton*>(button)) {
                enabled = enableButton;
            } else if (button->getName() == "Set selected vertex as loop start") {
                loopMarker = button;
            } else if (button->getName() == "Set selected vertex as sustain point") {
                sustainMarker = button;
            } else if (button->getName() == "Fit envelope vertical range") {
                fitVertical = button;
            } else if (button->getName() == "Show full envelope vertical range") {
                fullVertical = button;
            }
        }
    }
    REQUIRE(modeSelector != nullptr);
    REQUIRE(axisScaleSelector != nullptr);
    REQUIRE(modeSelector->getNumChildComponents() == 4);
    REQUIRE(pitchMode != nullptr);
    REQUIRE(scratchMode != nullptr);
    REQUIRE(loopMarker != nullptr);
    REQUIRE(sustainMarker != nullptr);
    REQUIRE(fitVertical != nullptr);
    REQUIRE(fullVertical != nullptr);
    REQUIRE(enabled != nullptr);
    REQUIRE(enabled->getToggleState());
    REQUIRE(enabled->getTooltip() == "Enable or disable this Envelope layer");
    REQUIRE(enabled->getBounds().toFloat() == embeddedEditorHeaderLayout(
            editor->getLocalBounds().toFloat(), true).enabled);
    REQUIRE(axisScaleSelector->getNumChildComponents() == 2);
    REQUIRE_FALSE(axisScaleSelector->isEnabled());
    REQUIRE_FALSE(axisScaleSelector->isVisible());
    REQUIRE_FALSE((bool) state.getProperty("axisScaleVisible", true));
    REQUIRE((bool) state.getProperty("actionIconsVector", {}));
    REQUIRE_FALSE(loopMarker->isEnabled());
    REQUIRE_FALSE(sustainMarker->isEnabled());
    REQUIRE(loopMarker->getTooltip().containsIgnoreCase("select one envelope vertex"));
    REQUIRE(sustainMarker->getTooltip().containsIgnoreCase("select one envelope vertex"));
    const auto fitBounds = rectangleProperty(state, "fitVerticalBounds");
    const auto fullBounds = rectangleProperty(state, "fullVerticalBounds");
    const auto modeBounds = rectangleProperty(state, "modeBounds");
    REQUIRE(state.getProperty("modeLabel", {}).toString() == "Purpose");
    REQUIRE_FALSE(state.hasProperty("markerGroupLabel"));
    REQUIRE_FALSE(state.hasProperty("axisScaleGroupLabelBounds"));
    REQUIRE_FALSE(state.hasProperty("rangeGroupLabelBounds"));
    REQUIRE_FALSE((bool) state.getProperty("loopEnabled", {}));
    REQUIRE_FALSE((bool) state.getProperty("sustainEnabled", {}));
    REQUIRE(modeBounds == purposeBounds);
    const var modeOptions = state.getProperty("modeOptions", {});
    REQUIRE(modeOptions.isArray());
    REQUIRE(modeOptions.getArray()->size() == 4);
    for (const auto& option : *modeOptions.getArray()) {
        const auto optionBounds = rectangleProperty(option, "bounds");
        const auto labelBounds = rectangleProperty(option, "labelBounds");
        REQUIRE(optionBounds.getWidth() == Catch::Approx(70.f));
        REQUIRE(optionBounds.getHeight() == Catch::Approx(30.f));
        REQUIRE(labelBounds.getWidth() == Catch::Approx(62.f));
        REQUIRE(labelBounds.getHeight() == Catch::Approx(26.f));
        REQUIRE(labelBounds.getX() - optionBounds.getX() == Catch::Approx(4.f));
        REQUIRE(labelBounds.getY() - optionBounds.getY() == Catch::Approx(2.f));
    }
    REQUIRE(fitBounds.getWidth() == Catch::Approx(24.f));
    REQUIRE(fullBounds.getWidth() == Catch::Approx(fitBounds.getWidth()));
    const auto markerGroupBounds = rectangleProperty(state, "markerGroupBounds");
    const auto axisScaleBounds = rectangleProperty(state, "axisScaleBounds");
    const auto rangeGroupBounds = rectangleProperty(state, "rangeGroupBounds");
    for (const auto group : { axisScaleBounds, rangeGroupBounds, markerGroupBounds }) {
        REQUIRE(panelBounds.contains(group));
        REQUIRE(group.getY() == Catch::Approx(panelBounds.getY() + 8.f));
        REQUIRE(group.getHeight() == Catch::Approx(24.f));
    }
    REQUIRE(rangeGroupBounds.getX() - axisScaleBounds.getRight() == Catch::Approx(16.f));
    REQUIRE(markerGroupBounds.getX() - rangeGroupBounds.getRight() == Catch::Approx(16.f));
    REQUIRE(rangeGroupBounds.contains(fitBounds));
    REQUIRE(rangeGroupBounds.contains(fullBounds));
    const auto requireActionIconMetrics = [&state](
            const char* controlProperty,
            const char* iconProperty) {
        const auto control = rectangleProperty(state, controlProperty);
        const auto icon = rectangleProperty(state, iconProperty);
        REQUIRE(control.getWidth() == Catch::Approx(24.f));
        REQUIRE(control.getHeight() == Catch::Approx(24.f));
        REQUIRE(icon.getWidth() == Catch::Approx(14.5f));
        REQUIRE(icon.getHeight() == Catch::Approx(14.5f));
        REQUIRE(icon.getX() - control.getX() == Catch::Approx(4.75f));
        REQUIRE(icon.getY() - control.getY() == Catch::Approx(4.75f));
    };
    requireActionIconMetrics("loopBounds", "loopIconBounds");
    requireActionIconMetrics("sustainBounds", "sustainIconBounds");
    requireActionIconMetrics("fitVerticalBounds", "fitVerticalIconBounds");
    requireActionIconMetrics("fullVerticalBounds", "fullVerticalIconBounds");
    const var axisScaleOptions = state.getProperty("axisScaleOptions", {});
    REQUIRE(axisScaleOptions.isArray());
    REQUIRE(axisScaleOptions.getArray()->size() == 2);
    for (const auto& option : *axisScaleOptions.getArray()) {
        const auto optionBounds = rectangleProperty(option, "bounds");
        const auto diagramBounds = rectangleProperty(option, "diagramBounds");
        REQUIRE(optionBounds.getWidth() == Catch::Approx(24.f));
        REQUIRE(optionBounds.getHeight() == Catch::Approx(24.f));
        REQUIRE(diagramBounds.getWidth() == Catch::Approx(10.875f));
        REQUIRE(diagramBounds.getHeight() == Catch::Approx(10.875f));
        REQUIRE(diagramBounds.getX() - optionBounds.getX() == Catch::Approx(6.5625f));
        REQUIRE(diagramBounds.getY() - optionBounds.getY() == Catch::Approx(6.5625f));
    }
    const auto parameterRails = state.getProperty("vertexParameterRails", {});
    REQUIRE(parameterRails.isArray());
    REQUIRE(parameterRails.getArray()->size() == 7);
    REQUIRE((bool) state.getProperty("guideControlsVisible", false));
    REQUIRE(parameterRails.getArray()->getLast().getProperty("id", {}) == "guide.time");
    const auto morphLabelBounds = rectangleProperty(state, "morphGroupLabelBounds");
    const auto axisGroupLabelBounds = rectangleProperty(state, "axisGroupLabelBounds");
    REQUIRE(axisGroupLabelBounds.getWidth() == Catch::Approx(24.f));
    REQUIRE(axisGroupLabelBounds.getY() == Catch::Approx(morphLabelBounds.getY()));
    REQUIRE_FALSE(state.hasProperty("linkGroupLabelBounds"));
    const auto vertexParameterBounds = rectangleProperty(state, "vertexParameterBounds");
    REQUIRE(vertexParameterBounds.getHeight() >= 210.f);
    const auto planeLabelBounds = rectangleProperty(
            state, "morphPlaneGroupLabelBounds");
    const auto planeBounds = rectangleProperty(state, "morphPlaneBounds");
    REQUIRE(planeBounds.contains(planeLabelBounds));
    const auto firstRail = rectangleProperty(parameterRails.getArray()->getReference(0), "bounds");
    const auto secondRail = rectangleProperty(parameterRails.getArray()->getReference(1), "bounds");
    const auto thirdRail = rectangleProperty(parameterRails.getArray()->getReference(2), "bounds");
    const auto lastRail = rectangleProperty(parameterRails.getArray()->getLast(), "bounds");
    REQUIRE(secondRail.getY() - firstRail.getY()
            == Catch::Approx(thirdRail.getY() - secondRail.getY()));
    REQUIRE(panelBounds.getY() > lastRail.getBottom());
    REQUIRE((bool) panelState.getProperty("previewPreservesInteractiveZoom", {}));

    VertCube* selectedCube = envelopeModel.getMesh().getCubes().front();
    REQUIRE(selectedCube != nullptr);
    REQUIRE(envelopeModel.synchronizeFromMesh(selectedCube));
    REQUIRE(envelopeModel.selectedCubeId().has_value());
    Node selectedNode = *graph.findNode("env");
    selectedNode.model = CurveNodeModelState::copyOf(
            envelopeModel, 0.5f, 0.5f, selectedNode.model->revision() + 1);
    auto* selectedEditorState = new DynamicObject();
    selectedEditorState->setProperty(
            "selectedCubeId", (int64) *envelopeModel.selectedCubeId());
    selectedNode.editorState = var(selectedEditorState);
    editor->setNode(selectedNode);
    const var selectedState = editor->automationState();
    const auto* guideTargetRails = selectedState.getProperty(
            "vertexParameterRails", {}).getArray();
    REQUIRE(guideTargetRails != nullptr);
    REQUIRE_FALSE(guideTargetRails->isEmpty());
    const auto guideBounds = rectangleProperty(
            guideTargetRails->getLast(), "guideBounds");
    REQUIRE(guideBounds.getWidth() > 1.f);
    editor->mouseDown(curvePanelMouseEvent(
            *editor,
            guideBounds.getCentre(),
            ModifierKeys::leftButtonModifier,
            guideBounds.getCentre(),
            false));
    REQUIRE(delegate.guideMenuField.isNotEmpty());
    REQUIRE(delegate.guideMenuTarget
            == editor->localAreaToGlobal(guideBounds.toNearestInt()));
    REQUIRE((bool) selectedState.getProperty("loopEnabled", {}));
    REQUIRE((bool) selectedState.getProperty("sustainEnabled", {}));
    REQUIRE(loopMarker->getTooltip().containsIgnoreCase("toggle selected vertex"));
    REQUIRE(sustainMarker->getTooltip().containsIgnoreCase("toggle selected vertex"));

    fullVertical->onClick();
    panelState = widget.automationState();
    REQUIRE(static_cast<double>(panelState.getProperty("verticalZoomHeight", {}))
            == Catch::Approx(1.0));
    fitVertical->onClick();
    panelState = widget.automationState();
    REQUIRE(static_cast<double>(panelState.getProperty("verticalZoomHeight", {})) < 0.1);
    REQUIRE_FALSE(delegate.events.contains("publish"));
    scratchMode->onClick();
    REQUIRE(modeSelector->purpose() == EnvelopePurpose::Scratch);
    REQUIRE(scratchMode->getToggleState());
    REQUIRE_FALSE(pitchMode->getToggleState());
    REQUIRE_FALSE((bool) widget.automationState().getProperty("bipolar", {}));
    pitchMode->onClick();
    REQUIRE(modeSelector->purpose() == EnvelopePurpose::Pitch);
    REQUIRE(pitchMode->getToggleState());
    REQUIRE_FALSE(scratchMode->getToggleState());
    REQUIRE((bool) widget.automationState().getProperty("bipolar", {}));
    for (const auto purpose : { EnvelopePurpose::Volume, EnvelopePurpose::Control,
            EnvelopePurpose::Scratch, EnvelopePurpose::Pitch, EnvelopePurpose::Volume }) {
        modeSelector->setPurpose(purpose, sendNotificationSync);
        const bool visible = purpose == EnvelopePurpose::Volume;
        REQUIRE(axisScaleSelector->isVisible() == visible);
        const var nextState = editor->automationState();
        REQUIRE((bool) nextState.getProperty("axisScaleVisible", !visible) == visible);
        REQUIRE(rectangleProperty(nextState, "rangeGroupBounds") == rangeGroupBounds);
        REQUIRE(rectangleProperty(nextState, "markerGroupBounds") == markerGroupBounds);
    }
}

TEST_CASE("Envelope document reload frames the authored vertex bounds",
        "[cycle-v2][node-editor-host][envelope][zoom]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    GraphNodeFactory factory;
    Node node = factory.createNode(NodeKind::Envelope, "env", {});
    CurveEditorWidget widget(NodeKind::Envelope);
    widget.syncFromNode(node);
    REQUIRE(widget.prepareExpandedPanelComponent(
            node, Rectangle<float>(0.f, 0.f, 840.f, 684.f)) != nullptr);

    EnvelopeNodeModel model;
    const auto& cubes = model.getMesh().getCubes();
    for (int cubeIndex = 0; cubeIndex < (int) cubes.size(); ++cubeIndex) {
        for (int vertexIndex = 0; vertexIndex < (int) VertCube::numVerts; ++vertexIndex) {
            Vertex* vertex = cubes[(size_t) cubeIndex]->getVertex(vertexIndex);
            vertex->values[Vertex::Phase] = 4.f + 0.1f * cubeIndex;
            vertex->values[Vertex::Amp] = 0.45f + 0.01f * cubeIndex;
        }
    }
    REQUIRE(model.synchronizeFromMesh(nullptr));
    node.model = CurveNodeModelState::copyOf(model, 0.5f, 0.5f, model.revision() + 1);

    widget.resetDocumentPresentation();
    widget.syncFromNode(node);
    const var zoom = widget.automationState().getProperty("zoom", {});
    REQUIRE((double) zoom.getProperty("x", {}) > 3.5);
    REQUIRE((double) zoom.getProperty("w", {}) < 1.0);
    REQUIRE((double) zoom.getProperty("h", {}) < 0.5);
}

TEST_CASE("Envelope view retains depth lines without storage-only point markers",
        "[cycle-v2][node-editor-host][envelope][presentation]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    CurveEditorWidget widget(NodeKind::Envelope);
    widget.syncFromNode(node);
    REQUIRE(widget.prepareExpandedPanelComponent(
            node, Rectangle<float>(0.f, 0.f, 840.f, 684.f)) != nullptr);

    const var state = widget.automationState();
    const int sourceCount = state.getProperty("colorPointCount", {});
    const int visibleLineCount = state.getProperty("visibleColorLineCount", {});
    const int visibleCount = state.getProperty("visibleColorPointCount", {});
    REQUIRE(sourceCount > 0);
    REQUIRE(visibleLineCount == sourceCount);
    REQUIRE(visibleCount == 0);
    REQUIRE((bool) state.getProperty("redLinked", false));
    REQUIRE((bool) state.getProperty("blueLinked", false));
}

TEST_CASE("Hosted Envelope hover and box selection keep geometry and graph preparation unchanged",
        "[cycle-v2][editor][envelope][selection][hover][complexity][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const bool unrelatedState : { false, true }) {
        CAPTURE(unrelatedState);
        Component owner;
        owner.setSize(1100, 800);
        owner.addToDesktop(ComponentPeer::windowIsTemporary);
        owner.setVisible(true);
        auto graph = TestSupport::authoredEnvelopeGraph();
        if (unrelatedState) {
            addUnrelatedInteractionState(graph);
        }
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        RecordingPresentation presentation;
        EnvelopeEditorResources resources;
        auto& widget = *resources.curveEditorWidget(*document.graph().findNode("env"));
        widget.syncFromNode(*document.graph().findNode("env"));
        NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
        NodeEditorHost host(owner, commands, presentation, resources);
        REQUIRE(host.bind(document.graph().findNode("env"), owner.getLocalBounds(), document.revision()));
        auto* editor = dynamic_cast<EnvelopeEditorComponent*>(host.component());
        REQUIRE(editor != nullptr);
        auto* input = editor->getComponentAt(editor->panelBoundsForAutomation().getCentre().roundToInt());
        REQUIRE(dynamic_cast<PanelInputHostComponent*>(input) != nullptr);
        Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
        Graphics graphics(image);
        editor->paint(graphics);
        const auto initial = widget.automationState();
        const char* preparationCounters[] {
                "rasterizerRefreshCount", "sliceEvaluationCount", "panelPreparationCount", "panelLayoutCount"
        };
        for (const char* counter : preparationCounters) {
            REQUIRE(initial.hasProperty(counter));
        }
        const auto surfaceRebuilds = initial.getProperty("morphSurface", {}).getProperty("rebuildCount", {});
        const auto modelRevision = widget.modelRevision();
        const auto contentRevision = widget.contentRevision();
        const auto documentRevision = document.revision();
        const auto graphRevision = document.graph().getRevision();
        const auto model = document.graph().findNode("env")->model;
        int publications {};
        document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
        const auto zoom = initial.getProperty("zoom", {});
        const auto panelPoint = [&](float x, float y) {
            return Point<float>(input->getWidth() * (x - (float) zoom.getProperty("x", {}))
                            / (float) zoom.getProperty("w", {}),
                    input->getHeight() * (1.f - (y - (float) zoom.getProperty("y", {}))
                            / (float) zoom.getProperty("h", {})));
        };
        const auto unchangedPreparation = [&] {
            const auto counts = InteractionComplexityDiagnostics::counts();
            REQUIRE(counts.graphCopies == 0);
            REQUIRE(counts.audioSamplesCopied == 0);
            REQUIRE(counts.meshCopies == 0);
            REQUIRE(counts.modelSerializations == 0);
            REQUIRE(counts.validationNodeVisits == 0);
            REQUIRE(counts.validationEdgeVisits == 0);
            REQUIRE(document.revision() == documentRevision);
            REQUIRE(document.graph().getRevision() == graphRevision);
            REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
            REQUIRE(document.graph().findNode("env")->model == model);
            REQUIRE(widget.modelRevision() == modelRevision);
            REQUIRE(widget.contentRevision() == contentRevision);
            const auto panelState = widget.automationState();
            for (const char* counter : preparationCounters) {
                REQUIRE(panelState.getProperty(counter, {}) == initial.getProperty(counter, {}));
            }
            REQUIRE(widget.automationState().getProperty("morphSurface", {})
                    .getProperty("rebuildCount", {}) == surfaceRebuilds);
            REQUIRE(publications == 0);
            REQUIRE(presentation.recordedMovements == 0);
            REQUIRE(presentation.scheduledRefreshes == 0);
            REQUIRE(presentation.immediateRefreshes == 0);
            REQUIRE(presentation.gestureCommits == 0);
            REQUIRE(presentation.localCommits == 0);
            REQUIRE_FALSE(document.canUndo());
            REQUIRE_FALSE(document.isDirty());
        };
        const auto* intercepts = initial.getProperty("intercepts", {}).getArray();
        REQUIRE(intercepts != nullptr);
        REQUIRE(intercepts->size() >= 4);
        std::vector<float> hoveredPhases;
        for (const int index : { 1, intercepts->size() - 2 }) {
            const auto& intercept = intercepts->getReference(index);
            const auto position = panelPoint(intercept.getProperty("x", {}), intercept.getProperty("y", {}));
            InteractionComplexityDiagnostics::reset();
            input->mouseMove(curvePanelMouseEvent(*input, position, {}, position, false));
            editor->paint(graphics);
            unchangedPreparation();
            const auto state = widget.automationState();
            REQUIRE((bool) state.getProperty("hasCurrentCube", false));
            hoveredPhases.push_back(state.getProperty("currentVertex", {}).getProperty("x", {}));
        }
        REQUIRE(std::abs(hoveredPhases.front() - hoveredPhases.back()) > 0.05f);
        std::vector<float> knotPositions { 0.f, (float) input->getWidth() };
        for (const auto& intercept : *intercepts) {
            const float x = panelPoint(intercept.getProperty("x", {}), 0.f).x;
            if (x > 0.f && x < (float) input->getWidth()) {
                knotPositions.push_back(x);
            }
        }
        std::sort(knotPositions.begin(), knotPositions.end());
        float gapStart {};
        float gapWidth {};
        for (size_t index = 1; index < knotPositions.size(); ++index) {
            const float width = knotPositions[index] - knotPositions[index - 1];
            if (width > gapWidth) {
                gapStart = knotPositions[index - 1];
                gapWidth = width;
            }
        }
        REQUIRE(gapWidth > 20.f);
        const Point<float> source(gapStart + gapWidth * 0.2f, input->getHeight() * 0.1f);
        REQUIRE(input->getLocalBounds().toFloat().contains(source));
        const ModifierKeys boxModifiers(ModifierKeys::leftButtonModifier | ModifierKeys::shiftModifier);
        InteractionComplexityDiagnostics::reset();
        input->mouseDown(curvePanelMouseEvent(*input, source, boxModifiers, source, false));
        REQUIRE((bool) widget.automationState().getProperty("boxSelecting", false));
        unchangedPreparation();
        Point<float> destination;
        for (const float width : { 0.5f, 0.8f }) {
            destination = { gapStart + gapWidth * width, input->getHeight() * 0.9f };
            REQUIRE(input->getLocalBounds().toFloat().contains(destination));
            InteractionComplexityDiagnostics::reset();
            input->mouseDrag(curvePanelMouseEvent(*input, destination, boxModifiers, source, true));
            editor->paint(graphics);
            REQUIRE((bool) widget.automationState().getProperty("boxSelecting", false));
            unchangedPreparation();
        }
        REQUIRE(widget.automationState().getProperty("movingVertexCount", {})
                == initial.getProperty("movingVertexCount", {}));
        InteractionComplexityDiagnostics::reset();
        input->mouseUp(curvePanelMouseEvent(*input, destination, {}, source, true));
        editor->paint(graphics);
        unchangedPreparation();
        REQUIRE_FALSE(dynamic_cast<PanelInputHostComponent*>(input)->isPointerGestureActive());
        REQUIRE_FALSE(dispatcher.hasTransientEdit());
    }
}

TEST_CASE("Envelope preview sync defers selection work until host initialization",
        "[cycle-v2][node-editor-host][envelope][preview][preset]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    GraphNodeFactory factory;
    Node envelope = factory.createNode(NodeKind::Envelope, "env", {});
    EnvelopeNodeModel envelopeModel;
    VertCube* selectedCube = envelopeModel.getMesh().getCubes().front();
    REQUIRE(selectedCube != nullptr);
    REQUIRE(envelopeModel.synchronizeFromMesh(selectedCube));
    envelope.model = CurveNodeModelState::copyOf(
            envelopeModel, 0.5f, 0.5f, envelopeModel.revision() + 1);
    auto* editorState = new DynamicObject();
    editorState->setProperty(
            "selectedCubeId", (int64) *envelopeModel.selectedCubeId());
    envelope.editorState = var(editorState);

    CurveEditorWidget widget(NodeKind::Envelope);
    widget.syncFromNode(envelope);

    REQUIRE(widget.getExpandedPanelComponentIfCreated() == nullptr);
    REQUIRE_FALSE((bool) widget.automationState().getProperty("hasCurrentCube", {}));

    REQUIRE(widget.prepareExpandedPanelComponent(
            envelope,
            Rectangle<float>(0.f, 0.f, 640.f, 400.f)) != nullptr);
    const var initializedState = widget.automationState();
    REQUIRE((bool) initializedState.getProperty("hasCurrentCube", {}));
    REQUIRE((int) initializedState.getProperty("movingVertexCount", 0) == 1);
    REQUIRE((int) initializedState.getProperty("physicalMovingVertexCount", 0) == 8);
}

TEST_CASE("Logarithmic Envelope grid distinguishes major divisions",
        "[cycle-v2][node-editor-host][envelope][logarithmic][grid]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));

    CurveEditorWidget widget(NodeKind::Envelope);
    auto editor = createCurveNodeEditor(NodeKind::Envelope, widget);
    RecordingCurveDelegate delegate;
    editor->setDelegate(&delegate);
    editor->setBounds(0, 0, 640, 400);
    editor->setNode(*graph.findNode("env"));

    EnvelopeAxisScaleSelector* selector {};
    for (auto* child : editor->getChildren()) {
        selector = dynamic_cast<EnvelopeAxisScaleSelector*>(child);
        if (selector != nullptr) {
            break;
        }
    }
    REQUIRE(selector != nullptr);
    REQUIRE(selector->isEnabled());
    REQUIRE_FALSE(selector->isLogarithmic());

    Button* logarithmic {};
    Button* linear {};
    for (auto* child : selector->getChildren()) {
        auto* button = dynamic_cast<Button*>(child);
        REQUIRE(button != nullptr);
        if (button->getName() == "Logarithmic axis scale") {
            logarithmic = button;
        } else if (button->getName() == "Linear axis scale") {
            linear = button;
        }
    }
    REQUIRE(logarithmic != nullptr);
    REQUIRE(linear != nullptr);

    logarithmic->onClick();
    REQUIRE(selector->isLogarithmic());
    REQUIRE(logarithmic->getToggleState());
    REQUIRE_FALSE(linear->getToggleState());
    REQUIRE(delegate.events.contains("publish"));

    var panelState = widget.automationState();
    REQUIRE((int) panelState.getProperty("horizontalMinorGridLineCount", {}) == 12);
    REQUIRE((int) panelState.getProperty("horizontalMajorGridLineCount", {}) == 4);
    REQUIRE(static_cast<double>(panelState.getProperty("minorGridBrightness", {}))
            == Catch::Approx(0.085 * 1.2));
    REQUIRE(static_cast<double>(panelState.getProperty("majorGridBrightness", {}))
            == Catch::Approx(0.14));

    linear->onClick();
    REQUIRE_FALSE(selector->isLogarithmic());
    REQUIRE(linear->getToggleState());
    REQUIRE_FALSE(logarithmic->getToggleState());
}

TEST_CASE("Node editor command service publishes a curve drag as one transaction") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 1, 0.f, 0.f, 1.f },
            { 2, 1.f, 1.f, 1.f }
    }));
    model.setPublicationRevision(document.graph().findNode("shape")->model->revision() + 1);

    commands.beginCurveTransaction();
    REQUIRE(commands.publishCurveState(
            "shape",
            CurveNodeModelState::copyOf(model, model.revision()),
            curveControls(*document.graph().findNode("shape"))));
    REQUIRE(model.replaceVertices({
            { 1, 0.f, 0.25f, 1.f },
            { 2, 1.f, 0.75f, 1.f }
    }));
    model.setPublicationRevision(document.graph().findNode("shape")->model->revision() + 1);
    auto secondControls = curveControls(*document.graph().findNode("shape"));
    for (auto& control : secondControls) {
        if (control.id == "post") {
            control.value = "0.9";
        }
    }
    REQUIRE(commands.publishCurveState(
            "shape",
            CurveNodeModelState::copyOf(model, model.revision()),
            secondControls));
    REQUIRE(presentation.scheduledRefreshes == 0);
    REQUIRE(presentation.repaints == 2);
    commands.commitCurveTransaction();

    REQUIRE(presentation.scheduledRefreshes == 1);
    REQUIRE(presentation.repaints == 3);
    const auto* committed = dynamic_cast<const CurveNodeModelState*>(
            document.graph().findNode("shape")->model.get());
    REQUIRE(committed != nullptr);
    REQUIRE(committed->flatCurve() != nullptr);
    REQUIRE(committed->flatCurve()->getVertices()
            == std::vector<FlatCurveVertex> {
                    { 1, 0.f, 0.25f, 1.f },
                    { 2, 1.f, 0.75f, 1.f }
            });
    REQUIRE(parameterValueForNode(*document.graph().findNode("shape"), "post") == "0.9");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("shape"), "post") == "0.5");
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Live curve point commit refreshes the graph after local movement",
        "[cycle-v2][editor][curve][gesture]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Waveshaper, "shape", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    presentation.refreshMode = ProbeRefreshMode::LiveLatest;
    NullResources resources;
    NodeEditorCommandService commands(
            owner, document, dispatcher, presentation, resources);
    FlatCurveModel model;
    REQUIRE(model.replaceVertices({
            { 1, 0.f, 0.f, 1.f },
            { 2, 1.f, 0.75f, 1.f }
    }));
    model.setPublicationRevision(document.graph().findNode("shape")->model->revision() + 1);

    commands.beginCurveTransaction();
    REQUIRE(commands.publishCurveState(
            "shape",
            CurveNodeModelState::copyOf(model, model.revision()),
            curveControls(*document.graph().findNode("shape"))));
    REQUIRE(presentation.scheduledRefreshes == 0);
    commands.commitCurveTransaction();

    REQUIRE(presentation.localCommits == 1);
    REQUIRE(presentation.scheduledRefreshes == 1);
    REQUIRE(document.canUndo());
}

TEST_CASE("Waveshaper point previews commit once and cancel without undo",
        "[cycle-v2][node-editor-host][waveshaper][gesture][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    for (const ProbeRefreshMode mode : {
            ProbeRefreshMode::LiveLatest,
            ProbeRefreshMode::OnGestureCommit }) {
        for (const bool scaled : { false, true }) {
            CAPTURE((int) mode, scaled);
            NodeGraph graph;
            graph.addNode(GraphNodeFactory().createNode(NodeKind::Waveshaper, "shape", {}));
            if (scaled) {
                addUnrelatedInteractionState(graph);
            }
            GraphDocument document(std::move(graph));
            GraphCommandDispatcher dispatcher(document);
            Component owner;
            RecordingPresentation presentation;
            presentation.refreshMode = mode;
            NullResources resources;
            NodeEditorCommandService commands(
                    owner, document, dispatcher, presentation, resources);
            const Node* originalNode = document.graph().findNode("shape");
            REQUIRE(originalNode != nullptr);
            const auto original = std::dynamic_pointer_cast<const CurveNodeModelState>(
                    originalNode->model);
            REQUIRE(original != nullptr);
            const FlatCurveVertex point = original->flatCurve()->getVertices()[1];
            const auto controls = curveControls(*originalNode);
            const auto originalConfiguration = WaveshaperSignalProcessor::buildConfiguration(
                    controls, original);
            REQUIRE(originalConfiguration != nullptr);

            commands.beginCurveTransaction();
            InteractionComplexityDiagnostics::reset();
            for (const float y : { 0.25f, 0.35f }) {
                const FlatCurveVertex moved { point.id, 0.2f, y, point.curve };
                const auto preview = original->withPointPreview(
                        moved, original->revision() + 1);
                REQUIRE(commands.publishCurveState("shape", preview, controls));
                const auto visible = std::dynamic_pointer_cast<const CurveNodeModelState>(
                        dispatcher.editingGraph().findNode("shape")->model);
                REQUIRE(visible != nullptr);
                REQUIRE(visible->pointPreview() == moved);
                REQUIRE(document.graph().findNode("shape")->model == original);
                const auto workerSnapshot = dispatcher.snapshotTransientEditForWorker();
                REQUIRE(workerSnapshot != nullptr);
                const auto workerModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
                        workerSnapshot->findNode("shape")->model);
                REQUIRE(workerModel != nullptr);
                REQUIRE(workerModel->pointPreview() == moved);
            }
            const auto movementCounts = InteractionComplexityDiagnostics::counts();
            REQUIRE(movementCounts.graphCopies == 0);
            REQUIRE(movementCounts.meshCopies == 0);
            REQUIRE(movementCounts.modelSerializations == 0);
            REQUIRE(movementCounts.audioSamplesCopied == 0);
            REQUIRE(movementCounts.nodeLinearScans == 0);
            REQUIRE(movementCounts.parameterLinearScans == 2);
            REQUIRE(movementCounts.validationNodeVisits == 0);
            REQUIRE(movementCounts.validationEdgeVisits == 0);
            const auto previewConfiguration = WaveshaperSignalProcessor::buildConfiguration(
                    controls,
                    dispatcher.editingGraph().findNode("shape")->model);
            REQUIRE(previewConfiguration != nullptr);
            REQUIRE(previewConfiguration->transfer->lookup(0.2f)
                    != Catch::Approx(originalConfiguration->transfer->lookup(0.2f)));

            FlatCurveModel finalCurve;
            REQUIRE(finalCurve.copyFrom(*original->flatCurve()));
            REQUIRE(finalCurve.moveVertex(point.id, { 0.2f, 0.35f }).succeeded());
            const auto finalModel = CurveNodeModelState::copyOf(
                    finalCurve, original->revision() + 1);
            REQUIRE(commands.publishCurveState("shape", finalModel, controls));
            commands.commitCurveTransaction();
            REQUIRE(document.graph().findNode("shape")->model->equals(*finalModel));
            REQUIRE(document.canUndo());
            const NodeGraph loaded = GraphSerializer().fromJsonString(document.toJson());
            REQUIRE(loaded.findNode("shape") != nullptr);
            REQUIRE(loaded.findNode("shape")->model->equals(*finalModel));
            REQUIRE(document.undo());
            REQUIRE(document.graph().findNode("shape")->model->equals(*original));
            REQUIRE_FALSE(document.canUndo());
            REQUIRE(document.redo());

            commands.beginCurveTransaction();
            const auto committed = std::dynamic_pointer_cast<const CurveNodeModelState>(
                    document.graph().findNode("shape")->model);
            REQUIRE(committed != nullptr);
            const auto cancelled = committed->withPointPreview(
                    { point.id, 0.3f, 0.45f, point.curve },
                    finalModel->revision() + 1);
            REQUIRE(commands.publishCurveState("shape", cancelled, controls));
            REQUIRE(commands.cancelCurveTransaction());
            REQUIRE(document.graph().findNode("shape")->model->equals(*finalModel));
            REQUIRE(document.undo());
            REQUIRE_FALSE(document.canUndo());
        }
    }
}

TEST_CASE("Node editor resource commands use the atomic dispatcher boundary",
        "[cycle-v2][node-editor-host][audio-resource]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.setNodeAudioResource({
            "ir",
            { "audio-1", "room.wav", 48000.0, { 1.f, 0.f } },
            "direct"
    }));
    REQUIRE(document.graph().findAudioResourceBinding("ir") != nullptr);
    REQUIRE(presentation.immediateRefreshes == 0);
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(presentation.rebinds == 1);

    REQUIRE(commands.removeNodeAudioResource("ir"));
    REQUIRE(document.graph().findAudioResourceBinding("ir") == nullptr);
    REQUIRE(presentation.immediateRefreshes == 2);
    REQUIRE(presentation.rebinds == 2);
}

TEST_CASE("Typed model gestures retain a durable base across updates, commit, and undo",
        "[cycle-v2][editor][model][causal]") {
    ScopedJuceInitialiser_GUI juce;
    for (const auto mode : { ProbeRefreshMode::OnGestureCommit, ProbeRefreshMode::LiveLatest }) {
        for (const int unrelatedNodes : { 0, 128 }) {
            CAPTURE(mode, unrelatedNodes);
            Component owner;
            NodeGraph graph;
            GraphNodeFactory factory;
            Node unison = factory.createNode(NodeKind::Unison, "unison", {});
            for (auto& parameter : unison.parameters) {
                if (parameter.id == "mode") {
                    parameter.value = "individual";
                }
            }
            graph.addNode(std::move(unison));
            for (int index = 0; index < unrelatedNodes; ++index) {
                graph.addNode(factory.createNode(NodeKind::Delay, "other" + String(index), {}));
            }
            REQUIRE(graph.addAudioResource({
                    "unrelated", "unrelated.wav", 48000.0,
                    std::vector<float>((size_t) unrelatedNodes * 128, 0.5f)
            }));
            GraphDocument document(std::move(graph));
            GraphCommandDispatcher dispatcher(document);
            RecordingPresentation presentation;
            presentation.refreshMode = mode;
            NullResources resources;
            NodeEditorCommandService commands(
                    owner, document, dispatcher, presentation, resources);
            const auto initial = document.graph().findNode("unison")->model;
            const uint64_t durableRevision = document.revision();
            REQUIRE(commands.beginNodeModelEdit("unison"));
            InteractionComplexityDiagnostics::reset();

            REQUIRE(commands.publishNodeModel(
                    "unison", UnisonNodeModelState::create({ { 0.5f, 0.2f, 0.f } }, 2)));
            REQUIRE(document.revision() == durableRevision);
            REQUIRE(document.graph().findNode("unison")->model == initial);
            REQUIRE(commands.publishNodeModel(
                    "unison", dispatcher.editingGraph().findNode("unison")->model));
            REQUIRE(presentation.recordedMovements == 1);
            REQUIRE(commands.publishNodeModel(
                    "unison", UnisonNodeModelState::create({ { 0.5f, 0.8f, 0.f } }, 2)));
            REQUIRE(document.graph().findNode("unison")->model == initial);
            const Node* transient = dispatcher.editingGraph().findNode("unison");
            const auto transientConfiguration = buildUnisonNodeConfiguration(
                    transient->parameters, transient->model);
            REQUIRE(transientConfiguration->layout.voices.front().pan == Catch::Approx(0.8f));
            commands.endNodeModelEdit();

            REQUIRE(document.revision() == durableRevision + 1);
            REQUIRE(presentation.recordedMovements == 2);
            REQUIRE(presentation.gestureCommits == 1);
            REQUIRE(presentation.scheduledRefreshes == 0);
            REQUIRE(presentation.immediateRefreshes == 0);
            REQUIRE_FALSE(dispatcher.hasTransientEdit());
            const Node* committed = document.graph().findNode("unison");
            REQUIRE(buildUnisonNodeConfiguration(committed->parameters, committed->model)
                    ->layout.voices.front().pan == Catch::Approx(0.8f));
            REQUIRE(document.undo());
            REQUIRE(document.graph().findNode("unison")->model == initial);
            REQUIRE_FALSE(document.canUndo());
            REQUIRE(document.redo());
            REQUIRE(document.graph().findNode("unison")->model->revision() == 2);
            const auto counts = InteractionComplexityDiagnostics::counts();
            REQUIRE(counts.graphCopies == 0);
            REQUIRE(counts.meshCopies == 0);
            REQUIRE(counts.audioSamplesCopied == 0);
            REQUIRE(counts.modelSerializations == 0);
            REQUIRE(counts.nodeLinearScans == 0);
        }
    }
}

TEST_CASE("Discrete typed model commands use one gesture and reject unrelated publications",
        "[cycle-v2][editor][model][causal]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    GraphNodeFactory factory;
    graph.addNode(factory.createNode(NodeKind::Unison, "unison", {}));
    graph.addNode(factory.createNode(NodeKind::Unison, "other", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
    REQUIRE(commands.publishNodeModel("unison", UnisonNodeModelState::create({ {}, {} }, 2)));
    REQUIRE_FALSE(dispatcher.hasTransientEdit());
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.canUndo());

    REQUIRE(commands.beginNodeModelEdit("unison"));
    REQUIRE_FALSE(commands.publishNodeModel("other", UnisonNodeModelState::create({ {}, {} }, 2)));
    REQUIRE(commands.publishNodeModel("unison", UnisonNodeModelState::create({ {}, {} }, 2)));
    REQUIRE_FALSE(commands.publishNodeModel("unison", UnisonNodeModelState::create({ {}, {}, {} }, 1)));
    REQUIRE_FALSE(dispatcher.hasTransientEdit());
    REQUIRE(document.graph().findNode("unison")->model->revision() == 1);
    commands.endNodeModelEdit();
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Trimesh primary morph navigation preserves graph and selected vertex",
        "[cycle-v2][editor][trimesh][causal]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    Component owner;
    NodeGraph graph = TestSupport::authoredTrimeshGraph();
    auto editorState = std::make_unique<DynamicObject>();
    editorState->setProperty("selectedVertexId", 2);
    REQUIRE(GraphNodeStateEditor().setNodeEditorState(
            graph, "mesh", var(editorState.release())).succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    widget.syncFromNode(*document.graph().findNode("mesh"));
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    const int selectedVertex = widget.selectedVertexIndexForPanel();
    const auto selectedParameters = widget.vertexParametersForIndex(selectedVertex);
    REQUIRE(selectedVertex == 2);
    REQUIRE(selectedParameters.size() == 6);
    const uint64_t documentRevision = document.revision();
    const uint64_t graphRevision = document.graph().getRevision();
    const auto model = document.graph().findNode("mesh")->model;

    widget.setMorphEditGestureActive(true);
    REQUIRE(commands.beginTrimeshMorphEdit("mesh", "yellow", 0.6f));
    REQUIRE(commands.updateTrimeshMorphEditValue(0.7f));
    REQUIRE(commands.updateTrimeshMorphEditValue(0.8f));
    REQUIRE(widget.selectedVertexIndexForPanel() == selectedVertex);
    const auto morphedParameters = widget.vertexParametersForIndex(selectedVertex);
    REQUIRE(morphedParameters.size() == selectedParameters.size());
    for (size_t i = 0; i < selectedParameters.size(); ++i) {
        REQUIRE(morphedParameters[i].id == selectedParameters[i].id);
        REQUIRE(morphedParameters[i].value == Catch::Approx(selectedParameters[i].value));
    }
    commands.endTrimeshMorphEdit();
    widget.setMorphEditGestureActive(false);

    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "yellow") == "0");
    REQUIRE(document.revision() == documentRevision);
    REQUIRE(document.graph().getRevision() == graphRevision);
    REQUIRE(document.graph().findNode("mesh")->model == model);
    REQUIRE_FALSE(dispatcher.hasTransientEdit());
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(presentation.immediateRefreshes == 0);
}

TEST_CASE("Rejected Trimesh vertex collision leaves no undoable edit",
        "[cycle-v2][editor][trimesh][collision]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("collision-test");
    node.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    mesh->destroy();
    auto editorState = std::make_unique<DynamicObject>();
    editorState->setProperty("selectedVertexId", 0);
    node.editorState = var(editorState.release());
    graph.addNode(std::move(node));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    widget.syncFromNode(*document.graph().findNode("mesh"));
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner, document, dispatcher, presentation, resources);
    const uint64_t initialRevision = document.graph().findNode("mesh")->model->revision();
    const float initialPhase = widget.currentMesh().getVerts().front()->values[Vertex::Phase];

    REQUIRE_FALSE(commands.beginTrimeshVertexParameterEdit(
            "mesh", "vertex.phase", 0.5f));
    commands.endTrimeshVertexParameterEdit();

    REQUIRE(widget.currentMesh().getVerts().front()->values[Vertex::Phase]
            == initialPhase);
    REQUIRE(document.graph().findNode("mesh")->model->revision() == initialRevision);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(presentation.recordedMovements == 0);

    REQUIRE(commands.beginTrimeshVertexParameterEdit(
            "mesh", "vertex.phase", initialPhase));
    REQUIRE_FALSE(commands.updateTrimeshVertexParameterEditValue(0.5f));
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(initialPhase + 0.01f));
    commands.endTrimeshVertexParameterEdit();
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    const auto restored = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->mesh().getVerts().front()->values[Vertex::Phase]
            == initialPhase);
}

TEST_CASE("Trimesh guide gain gesture publishes prepared gain and undoes as one edit",
        "[cycle-v2][editor][trimesh][guide][gain][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    addUnrelatedInteractionState(graph);
    const auto guide = GuideGraphEditor().createGuideCurve(graph);
    REQUIRE(guide.succeeded());
    REQUIRE(GuideGraphEditor().assignGuideCurveToMeshComponent(
            graph,
            guide.nodeId,
            "mesh",
            0,
            "amp").succeeded());

    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    widget.syncFromNode(*document.graph().findNode("mesh"));
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.beginTrimeshVertexParameterEdit(
            "mesh", "guideGain.amp", 0.5f));
    const uint64_t revisionBeforeMovement = document.revision();
    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.7f));
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.8f));
    REQUIRE(document.revision() == revisionBeforeMovement);
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    commands.endTrimeshVertexParameterEdit();
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 1);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().audioSamplesCopied == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().assignmentLinearScans == 0);
    REQUIRE(document.revision() > revisionBeforeMovement);

    const Node* committedNode = document.graph().findNode("mesh");
    REQUIRE(committedNode != nullptr);
    const auto committed = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            committedNode->model);
    REQUIRE(committed != nullptr);
    REQUIRE(committed->mesh().getCubes().front()->guideCurveGainAt(Vertex::Amp)
            == Catch::Approx(0.8f));
    const auto prepared = TrimeshGuidePreparation::prepare(
            document.graph(), *committedNode, committed->mesh());
    REQUIRE(prepared.assignmentCount > 0);
    REQUIRE(prepared.mesh->getCubes().front()->guideCurveGainAt(Vertex::Amp)
            == Catch::Approx(0.8f));
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.immediateRefreshes == 0);
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(document.canUndo());

    REQUIRE(document.undo());
    const auto restored = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->mesh().getCubes().front()->guideCurveGainAt(Vertex::Amp)
            == Catch::Approx(0.5f));
    widget.syncFromNode(*document.graph().findNode("mesh"));
    REQUIRE(widget.vertexParametersForIndex(0)[4].guideGain
            == Catch::Approx(0.5f));
    REQUIRE_FALSE(document.canUndo());

    REQUIRE(commands.beginTrimeshVertexParameterEdit(
            "mesh", "guideGain.amp", 0.5f));
    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.7f));
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.5f));
    commands.endTrimeshVertexParameterEdit();
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    REQUIRE(presentation.immediateRefreshes == 0);
    REQUIRE(presentation.gestureCommits == 2);
    REQUIRE(document.graph().findNode("mesh")->model == restored);
    REQUIRE(document.canUndo());
}

TEST_CASE("Trimesh Curve gesture edits every selected cube corner and undoes",
        "[cycle-v2][editor][trimesh][curve-average]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    auto mesh = TrimeshMeshFactory::createDefaultMesh("curve-average-gesture");
    VertCube* sourceCube = mesh->getCubes().front();
    for (int index = 0; index < VertCube::numVerts; ++index) {
        sourceCube->getVertex(index)->values[Vertex::Curve] = index < 4 ? 0.f : 1.f;
    }
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    node.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    auto selectedState = std::make_unique<DynamicObject>();
    selectedState->setProperty("selectedVertexId", 0);
    node.editorState = var(selectedState.release());
    mesh->destroy();

    NodeGraph graph;
    graph.addNode(std::move(node));
    addUnrelatedInteractionState(graph);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    widget.syncFromNode(*document.graph().findNode("mesh"));
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner, document, dispatcher, presentation, resources);

    REQUIRE(commands.beginTrimeshVertexParameterEdit("mesh", "vertex.curve", 0.5f));
    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.25f));
    REQUIRE(commands.updateTrimeshVertexParameterEditValue(0.f));
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    for (int index = 0; index < VertCube::numVerts; ++index) {
        REQUIRE(widget.currentMesh().getCubes().front()
                ->getVertex(index)->values[Vertex::Curve] == 0.f);
    }
    commands.endTrimeshVertexParameterEdit();

    const auto committed = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(committed != nullptr);
    for (int index = 0; index < VertCube::numVerts; ++index) {
        REQUIRE(committed->mesh().getCubes().front()
                ->getVertex(index)->values[Vertex::Curve] == 0.f);
    }
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    const auto restored = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(restored != nullptr);
    for (int index = 0; index < VertCube::numVerts; ++index) {
        REQUIRE(restored->mesh().getCubes().front()
                ->getVertex(index)->values[Vertex::Curve] == (index < 4 ? 0.f : 1.f));
    }
}

TEST_CASE("Clicking an open Trimesh Guide selector dismisses its popup",
        "[cycle-v2][editor][trimesh][guide][menu]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    owner.setBounds(0, 0, 800, 600);
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.showTrimeshGuideAttachmentMenu(
            "mesh", "amp", { 100, 100, 40, 30 }));
    REQUIRE(commands.hasOpenTrimeshGuideAttachmentMenuFor("mesh", "amp"));

    REQUIRE(commands.showTrimeshGuideAttachmentMenu(
            "mesh", "amp", { 100, 100, 40, 30 }));
    REQUIRE_FALSE(commands.hasOpenTrimeshGuideAttachmentMenuFor("mesh", "amp"));
}

TEST_CASE("Trimesh morph shortcuts focus, reach poles, toggle links, and preserve Space",
        "[cycle-v2][editor][trimesh][keyboard]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    owner.setBounds(0, 0, 900, 620);
    owner.addToDesktop(ComponentPeer::windowIsTemporary);
    owner.setVisible(true);

    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "mesh", "dimensionCount", "Curve morphing", "3").succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(
            document.graph().findNode("mesh"),
            owner.getLocalBounds(),
            document.revision()));
    auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
    REQUIRE(editor != nullptr);
    REQUIRE_FALSE(editor->keyPressed(KeyPress(KeyPress::spaceKey)));

    REQUIRE(editor->keyPressed(KeyPress('r')));
    REQUIRE(editor->focusedMorphAxis() == "red");

    Component focusSink;
    focusSink.setWantsKeyboardFocus(true);
    editor->addAndMakeVisible(focusSink);
    focusSink.grabKeyboardFocus();
    REQUIRE(focusSink.hasKeyboardFocus(false));
    REQUIRE(editor->focusedMorphAxis() == "red");

    REQUIRE(owner.getPeer() != nullptr);
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::leftKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("red", 0.5f) == Catch::Approx(0.f));
    REQUIRE(document.undo());

    Component* blueMorph {};
    for (Component* child : editor->getChildren()) {
        blueMorph = child->findChildWithID("trimesh.morph.blue");
        if (blueMorph != nullptr) {
            break;
        }
    }
    REQUIRE(blueMorph != nullptr);
    REQUIRE(editor->keyPressed(KeyPress('b')));
    const float initialBlue = NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("blue", 0.5f);
    blueMorph->grabKeyboardFocus();
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress('y')));
    REQUIRE(editor->focusedMorphAxis() == "yellow");
    REQUIRE(blueMorph->hasKeyboardFocus(false));
    const float initialYellow = NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("yellow", 0.5f);
    const auto yellowRevision = document.revision();
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::rightKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("yellow", 0.5f) == Catch::Approx(initialYellow));
    REQUIRE(trimeshEditorMorphPosition(host)[0] == Catch::Approx(1.f));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("blue", 0.5f) == Catch::Approx(initialBlue));
    REQUIRE(editor->focusedMorphAxis() == "yellow");
    REQUIRE(document.revision() == yellowRevision);
    REQUIRE_FALSE(document.canUndo());

    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::downKey)));
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::downKey)));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::downKey)));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::upKey)));
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::leftKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("red", 0.5f) == Catch::Approx(0.f));
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(
            KeyPress::rightKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("red", 0.5f) == Catch::Approx(0.2f));
    REQUIRE(document.undo());
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("red", 0.5f) == Catch::Approx(0.f));
    REQUIRE(document.undo());

    REQUIRE(editor->keyPressed(KeyPress('r')));
    Component* redLink {};
    for (Component* child : editor->getChildren()) {
        redLink = child->findChildWithID("trimesh.link.red");
        if (redLink != nullptr) {
            break;
        }
    }
    REQUIRE(redLink != nullptr);
    redLink->grabKeyboardFocus();
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE(editor->keyPressed(KeyPress('l')));
    REQUIRE_FALSE(NodeParameterMap(*document.graph().findNode("mesh"))
            .boolValue("link.red", true));
    REQUIRE(document.undo());

    REQUIRE(editor->keyPressed(KeyPress('y')));
    REQUIRE(editor->focusedMorphAxis() == "yellow");
    REQUIRE(editor->keyPressed(KeyPress('b')));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(editor->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
            .floatValue("blue", 0.5f) == Catch::Approx(1.f));
}

TEST_CASE("Envelope morph shortcuts use curve transactions and session links",
        "[cycle-v2][editor][envelope][keyboard]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    owner.setBounds(0, 0, 900, 684);
    owner.addToDesktop(ComponentPeer::windowIsTemporary);
    owner.setVisible(true);

    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "env", "dimensionCount", "Curve morphing", "2").succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    EnvelopeEditorResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(
            document.graph().findNode("env"),
            owner.getLocalBounds(),
            document.revision()));
    auto* editor = dynamic_cast<EnvelopeEditorComponent*>(host.component());
    REQUIRE(editor != nullptr);
    REQUIRE_FALSE(editor->keyPressed(KeyPress(KeyPress::spaceKey)));
    REQUIRE_FALSE(editor->keyPressed(KeyPress('y')));

    REQUIRE(editor->keyPressed(KeyPress('r')));
    REQUIRE(editor->focusedMorphAxis() == "red");
    Component* shortcutHint = editor->findChildWithID(
            "envelopeEditor.morph.shortcuts");
    Component* shortcutBubble = editor->findChildWithID(
            "envelopeEditor.morph.shortcuts.bubble");
    REQUIRE(shortcutHint != nullptr);
    REQUIRE(shortcutBubble != nullptr);
    REQUIRE(shortcutHint->isVisible());
    REQUIRE(shortcutHint->getWidth() == 24);
    REQUIRE(shortcutHint->getHeight() == 24);
    REQUIRE_FALSE(shortcutBubble->isVisible());
    shortcutHint->mouseDown(curvePanelMouseEvent(
            *shortcutHint,
            { 12.f, 12.f },
            ModifierKeys::leftButtonModifier,
            { 12.f, 12.f },
            false));
    REQUIRE_FALSE(shortcutBubble->isVisible());
    shortcutHint->mouseEnter(curvePanelMouseEvent(
            *shortcutHint, { 12.f, 12.f }, {}, { 12.f, 12.f }, false));
    REQUIRE(shortcutBubble->isVisible());
    REQUIRE(shortcutBubble->getWidth() == 168);
    REQUIRE(shortcutBubble->getHeight() == 130);
    REQUIRE(editor->getLocalBounds().contains(shortcutBubble->getBounds()));
    REQUIRE(shortcutBubble->getBounds().getCentreX()
            == shortcutHint->getBounds().getCentreX());
    REQUIRE(shortcutBubble->getDescription().startsWith("Y / R / B"));
    REQUIRE(shortcutBubble->getDescription().contains(
            "Shift + Left / Right"));
    shortcutHint->mouseExit(curvePanelMouseEvent(
            *shortcutHint, { 25.f, 12.f }, {}, { 12.f, 12.f }, false));
    REQUIRE_FALSE(shortcutBubble->isVisible());

    Component focusSink;
    focusSink.setWantsKeyboardFocus(true);
    editor->addAndMakeVisible(focusSink);
    focusSink.grabKeyboardFocus();
    REQUIRE(focusSink.hasKeyboardFocus(false));
    REQUIRE(editor->focusedMorphAxis() == "red");

    REQUIRE(owner.getPeer() != nullptr);
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress(KeyPress::leftKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
            .floatValue("red", 0.5f) == Catch::Approx(0.f));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env")).intValue("dimensionCount", 0) == 2);
    REQUIRE(document.undo());
    REQUIRE(host.bind(
            document.graph().findNode("env"),
            owner.getLocalBounds(),
            document.revision()));

    Component* blueMorph = editor->findChildWithID("envelopeEditor.morph.blue");
    REQUIRE(blueMorph != nullptr);
    REQUIRE(editor->keyPressed(KeyPress('b')));
    blueMorph->grabKeyboardFocus();
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(owner.getPeer()->handleKeyPress(KeyPress('r')));
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE(blueMorph->hasKeyboardFocus(false));
    REQUIRE(blueMorph->keyPressed(KeyPress(KeyPress::leftKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
            .floatValue("red", 0.5f) == Catch::Approx(0.f));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
            .floatValue("blue", 0.5f) == Catch::Approx(0.5f));
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE(document.undo());
    REQUIRE(host.bind(
            document.graph().findNode("env"),
            owner.getLocalBounds(),
            document.revision()));

    REQUIRE(editor->keyPressed(KeyPress(KeyPress::downKey)));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(editor->keyPressed(KeyPress(KeyPress::downKey)));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(editor->keyPressed(KeyPress(KeyPress::upKey)));
    REQUIRE(editor->focusedMorphAxis() == "red");
    REQUIRE_FALSE(editor->keyPressed(KeyPress(
            KeyPress::upKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(editor->keyPressed(KeyPress(
            KeyPress::rightKey,
            ModifierKeys::shiftModifier,
            0)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
            .floatValue("red", 0.5f) == Catch::Approx(0.7f));
    REQUIRE(document.undo());
    REQUIRE(host.bind(
            document.graph().findNode("env"),
            owner.getLocalBounds(),
            document.revision()));

    REQUIRE(editor->keyPressed(KeyPress('r')));
    REQUIRE(editor->keyPressed(KeyPress('l')));
    DynamicObject state;
    host.appendAutomationState(state);
    const var effectState = state.getProperty("effect2D");
    const var panelState = effectState
            .getProperty("panelState", {});
    REQUIRE_FALSE((bool) panelState.getProperty("redLinked", true));
    REQUIRE_FALSE(document.isDirty());

    REQUIRE(editor->keyPressed(KeyPress('b')));
    REQUIRE(editor->focusedMorphAxis() == "blue");
    REQUIRE(editor->keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
            .floatValue("blue", 0.5f) == Catch::Approx(1.f));
}

TEST_CASE("Trimesh link toggles survive rebind and undo",
        "[cycle-v2][editor][trimesh][links]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "mesh", "dimensionCount", "Curve morphing", "3").succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);
    const Rectangle<int> bounds { 0, 0, 900, 620 };
    const auto redLinkSelected = [&host] {
        DynamicObject state;
        host.appendAutomationState(state);
        const Array<var>* links = state.getProperty("linkToggles").getArray();
        REQUIRE(links != nullptr);
        REQUIRE(links->size() == 3);
        REQUIRE((*links)[1].getProperty("id", {}).toString() == "red");
        return static_cast<bool>((*links)[1].getProperty("selected", {}));
    };
    const auto rebind = [&] {
        REQUIRE(host.bind(
                document.graph().findNode("mesh"),
                bounds,
                document.revision()));
    };

    rebind();
    REQUIRE(redLinkSelected());
    auto* enabled = dynamic_cast<ToggleButton*>(
            host.component()->findChildWithID("trimeshEditor.enabled"));
    REQUIRE(enabled != nullptr);
    REQUIRE(enabled->getToggleState());
    REQUIRE(enabled->getTooltip() == "Enable or disable this Trimesh layer");
    enabled->setToggleState(false, sendNotificationSync);
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "enabled") == "0");
    rebind();
    REQUIRE_FALSE(enabled->getToggleState());

    REQUIRE(commands.toggleTrimeshLinkAxisValue("mesh", "red"));
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "link.red") == "0");
    rebind();
    REQUIRE_FALSE(redLinkSelected());

    REQUIRE(commands.toggleTrimeshLinkAxisValue("mesh", "red"));
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "link.red") == "1");
    rebind();
    REQUIRE(redLinkSelected());

    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "link.red") == "0");
    rebind();
    REQUIRE_FALSE(redLinkSelected());
}

TEST_CASE("Spectral Trimesh range is visible and edits as one undo transaction",
        "[cycle-v2][editor][trimesh][range]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    resources.trimeshDomain = PortDomain::SpectralPhaseSignal;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(
            document.graph().findNode("mesh"),
            { 0, 0, 900, 620 },
            document.revision()));
    auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->showsOutputScale());

    DynamicObject state;
    host.appendAutomationState(state);
    REQUIRE((double) state.getProperty("range") == Catch::Approx(0.5));

    REQUIRE(commands.beginNodeParameterEdit("mesh", "range", "Range", 0.6f));
    REQUIRE(commands.updateNodeParameterEditValue(0.7f));
    commands.endNodeParameterEdit();
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "range") == "0.700000");
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "range") == "0.5");
}

TEST_CASE("Time Trimesh gain is visible and edits as one undo transaction",
        "[cycle-v2][editor][trimesh][gain]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    resources.trimeshDomain = PortDomain::TimeSignal;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(
            document.graph().findNode("mesh"),
            { 0, 0, 900, 620 },
            document.revision()));
    auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
    REQUIRE(editor != nullptr);
    REQUIRE(editor->showsOutputScale());
    REQUIRE(editor->outputScaleValue() == Catch::Approx(0.5f));

    REQUIRE(commands.beginNodeParameterEdit("mesh", "gain", "Gain", 0.6f));
    REQUIRE(commands.updateNodeParameterEditValue(0.7f));
    commands.endNodeParameterEdit();
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "gain") == "0.700000");
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "gain") == "0.5");
}

TEST_CASE("Trimesh signal type and polarity use shared segmented selectors",
        "[cycle-v2][editor][trimesh][signal-type]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    resources.trimeshDomain = PortDomain::SpectralMagnitudeSignal;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(
            document.graph().findNode("mesh"),
            { 0, 0, 900, 620 },
            document.revision()));
    auto* typeSelector = dynamic_cast<PropertySegmentedSelector*>(
            host.component()->findChildWithID("trimeshEditor.signalType"));
    REQUIRE(typeSelector != nullptr);
    REQUIRE(typeSelector->isVisible());
    REQUIRE(typeSelector->selectedValue() == "time");
    auto* magnitude = dynamic_cast<TextButton*>(typeSelector->findChildWithID(
            "trimeshEditor.signalType.spectralMagnitude"));
    REQUIRE(magnitude != nullptr);
    auto* time = dynamic_cast<TextButton*>(typeSelector->findChildWithID(
            "trimeshEditor.signalType.time"));
    REQUIRE(time != nullptr);

    magnitude->onClick();

    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "signalType") == "spectralMagnitude");
    auto* polaritySelector = dynamic_cast<PropertySegmentedSelector*>(
            host.component()->findChildWithID("trimeshEditor.polarity"));
    REQUIRE(polaritySelector != nullptr);
    REQUIRE(polaritySelector->isVisible());
    const Rectangle<int> magnitudeTypeBounds = typeSelector->getBounds();
    REQUIRE(polaritySelector->getRight() < magnitudeTypeBounds.getX());

    time->onClick();
    REQUIRE_FALSE(polaritySelector->isVisible());
    REQUIRE(typeSelector->getBounds() == magnitudeTypeBounds);

    magnitude->onClick();
    REQUIRE(polaritySelector->isVisible());
    REQUIRE(typeSelector->getBounds() == magnitudeTypeBounds);
    auto* bipolar = dynamic_cast<TextButton*>(polaritySelector->findChildWithID(
            "trimeshEditor.polarity.bipolar"));
    REQUIRE(bipolar != nullptr);
    bipolar->onClick();
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "polarity") == "bipolar");
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "polarity") == "unipolar");
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "signalType") == "time");
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "signalType") == "spectralMagnitude");
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "signalType") == "time");
    auto* phase = dynamic_cast<TextButton*>(typeSelector->findChildWithID(
            "trimeshEditor.signalType.spectralPhase"));
    REQUIRE(phase != nullptr);
    phase->onClick();
    auto* phaseModeSelector = dynamic_cast<PropertySegmentedSelector*>(
            host.component()->findChildWithID("trimeshEditor.phaseMode"));
    REQUIRE(phaseModeSelector != nullptr);
    REQUIRE(phaseModeSelector->isVisible());
    REQUIRE(phaseModeSelector->selectedValue() == "absolute");
    auto* velocity = dynamic_cast<TextButton*>(phaseModeSelector->findChildWithID(
            "trimeshEditor.phaseMode.velocity"));
    REQUIRE(velocity != nullptr);
    velocity->onClick();
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "phaseMode") == "velocity");
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("mesh"),
            "phaseMode") == "absolute");
}

TEST_CASE("Live primary Trimesh morph keeps graph previews deferred",
        "[cycle-v2][editor][trimesh][causal]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    presentation.refreshMode = ProbeRefreshMode::LiveLatest;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.beginTrimeshMorphEdit("mesh", "yellow", 0.8f));
    commands.endTrimeshMorphEdit();

    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "yellow") == "0");
    REQUIRE_FALSE(document.canUndo());
    REQUIRE_FALSE(dispatcher.hasTransientEdit());
    REQUIRE(presentation.immediateRefreshes == 0);
}

TEST_CASE("Live non-primary Trimesh morph commits one undoable gesture",
        "[cycle-v2][editor][trimesh][causal]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh, "mesh", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    presentation.refreshMode = ProbeRefreshMode::LiveLatest;
    NullResources resources;
    NodeEditorCommandService commands(
            owner, document, dispatcher, presentation, resources);
    const String initialRed = parameterValueForNode(
            *document.graph().findNode("mesh"), "red");

    REQUIRE(commands.beginTrimeshMorphEdit("mesh", "red", 0.3f));
    REQUIRE(commands.updateTrimeshMorphEditValue(0.6f));
    REQUIRE(commands.updateTrimeshMorphEditValue(0.8f));
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "red") == initialRed);
    commands.endTrimeshMorphEdit();

    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "red") == "0.800");
    REQUIRE(presentation.recordedMovements == 3);
    REQUIRE(presentation.gestureDownstreamFeedback == std::vector<bool> { false });
    REQUIRE(presentation.movementLocalProducts
            == std::vector<std::optional<UpdateProduct>>(3, std::nullopt));
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(presentation.localCommits == 0);
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "red") == initialRed);
}

TEST_CASE("Effect parameter drag publishes continuously as one undo transaction",
        "[cycle-v2][editor][effects]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Reverb, "reverb", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.beginNodeParameterEdit("reverb", "wet", "Wet", 0.55f));
    REQUIRE(commands.updateNodeParameterEditValue(0.7f));
    commands.endNodeParameterEdit();

    REQUIRE(parameterValueForNode(*document.graph().findNode("reverb"), "wet") == "0.700000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("reverb"), "wet") == "0.4");
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(presentation.gestureCommits == 1);
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.movementLocalProducts
            == std::vector<std::optional<UpdateProduct>>(
                    2, UpdateProduct::LocalSlice));
    REQUIRE(presentation.rebinds == 0);
    REQUIRE(presentation.transientRebinds == 0);
}

TEST_CASE("Voice Context hosted pitch gesture commits two updates and one undo",
        "[cycle-v2][editor][voice-context][gesture]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(document.graph().findNode("voice"), { 0, 0, 440, 318 }));
    auto* pitch = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "voiceContextEditor.pitch"));
    REQUIRE(pitch != nullptr);
    pitch->onDragStart();
    pitch->setValue(3.0, sendNotificationSync);
    pitch->setValue(9.0, sendNotificationSync);
    REQUIRE(parameterValueForNode(*dispatcher.editingGraph().findNode("voice"), "pitch") == "9.000000");
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "pitch") == "0");
    REQUIRE_FALSE(document.canUndo());

    pitch->onDragEnd();
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "pitch") == "9.000000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "pitch") == "0");
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.gestureCommits == 1);
}

TEST_CASE("Voice Context hosted length gesture commits two updates and one undo",
        "[cycle-v2][editor][voice-context][voice-length][gesture]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    presentation.refreshMode = ProbeRefreshMode::LiveLatest;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);

    REQUIRE(host.bind(document.graph().findNode("voice"), { 0, 0, 440, 318 }));
    auto* length = dynamic_cast<PrecisionSlider*>(host.component()->findChildWithID(
            "voiceContextEditor.voiceLength"));
    REQUIRE(length != nullptr);
    length->onDragStart();
    length->setValue(0.45, sendNotificationSync);
    length->setValue(0.65, sendNotificationSync);
    REQUIRE(parameterValueForNode(
            *dispatcher.editingGraph().findNode("voice"),
            "voiceLength") == "0.650000");
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("voice"),
            "voiceLength") == String(CycleDsp::voiceLengthUnitValue(1.0f)));
    REQUIRE_FALSE(document.canUndo());

    length->onDragEnd();
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("voice"),
            "voiceLength") == "0.650000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(
            *document.graph().findNode("voice"),
            "voiceLength") == String(CycleDsp::voiceLengthUnitValue(1.0f)));
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.gestureCommits == 1);
}

TEST_CASE("Unison drag exposes every transient preview before one undoable commit",
        "[cycle-v2][editor][effects][unison]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    Node unison = GraphNodeFactory().createNode(NodeKind::Unison, "unison", {});
    for (auto& parameter : unison.parameters) {
        if (parameter.id == "order") {
            parameter.value = "3";
        }
    }
    graph.addNode(std::move(unison));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    const String originalWidth = parameterValueForNode(
            *document.graph().findNode("unison"), "width");
    const uint64_t originalRevision = document.revision();
    std::vector<float> observedDetunes;
    const auto observePreview = [&] {
        const auto paths = UnisonPreviewPainter().makePaths(
                *dispatcher.editingGraph().findNode("unison"));
        REQUIRE_FALSE(paths.empty());
        observedDetunes.push_back(paths.back().detuneCents);
    };

    REQUIRE(commands.beginNodeParameterEdit(
            "unison", "width", "Width", originalWidth.getFloatValue()));
    REQUIRE(commands.updateNodeParameterEditValue(20.f));
    observePreview();
    REQUIRE(commands.updateNodeParameterEditValue(40.f));
    observePreview();
    REQUIRE(commands.updateNodeParameterEditValue(60.f));
    observePreview();

    REQUIRE(observedDetunes[0] < observedDetunes[1]);
    REQUIRE(observedDetunes[1] < observedDetunes[2]);
    REQUIRE(document.revision() == originalRevision);
    commands.endNodeParameterEdit();

    REQUIRE(parameterValueForNode(*document.graph().findNode("unison"), "width") == "60.000000");
    REQUIRE(presentation.recordedMovements == 3);
    REQUIRE(presentation.repaints == 3);
    REQUIRE(presentation.rebinds == 0);
    REQUIRE(presentation.transientRebinds == 0);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("unison"), "width") == originalWidth);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Trimesh drag keeps movement local and publishes one commit snapshot",
        "[cycle-v2][editor][trimesh][regression][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("TrimeshDragCommand");
    node.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    mesh->destroy();
    graph.addNode(std::move(node));
    auto editorState = std::make_unique<DynamicObject>();
    editorState->setProperty("selectedVertexId", 0);
    REQUIRE(GraphNodeStateEditor().setNodeEditorState(
            graph, "mesh", var(editorState.release())).succeeded());
    addUnrelatedInteractionState(graph);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    TrimeshWidget widget;
    widget.syncFromNode(*document.graph().findNode("mesh"));
    resources.activeTrimesh = &widget;

    const auto durableModel = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(durableModel != nullptr);
    REQUIRE_FALSE(durableModel->mesh().getVerts().empty());
    const float originalAmp = durableModel->mesh().getVerts().front()->values[Vertex::Amp];
    InteractionComplexityDiagnostics::reset();

    widget.currentMesh().getVerts().front()->values[Vertex::Amp] = originalAmp + 0.05f;
    commands.persistTrimeshMeshEdits("mesh", false);
    const auto firstTransient = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            dispatcher.editingGraph().findNode("mesh")->model);
    REQUIRE(firstTransient != nullptr);
    REQUIRE(firstTransient->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp));
    REQUIRE(widget.currentMesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp + 0.05f));
    const int transientSelection = widget.selectedVertexIndexForPanel();
    REQUIRE(transientSelection >= 0);
    REQUIRE(document.graph().findNode("mesh")->model->revision() == durableModel->revision());
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);

    widget.currentMesh().getVerts().front()->values[Vertex::Amp] = originalAmp + 0.1f;
    commands.persistTrimeshMeshEdits("mesh", true);
    const auto committed = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(committed != nullptr);
    REQUIRE(committed->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp + 0.1f));
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 1);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().audioSamplesCopied == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().assignmentLinearScans == 0);
    REQUIRE((int) document.graph().findNode("mesh")->editorState.getProperty(
            "selectedVertexId", -1) == transientSelection);
    REQUIRE(resources.synchronizingTrimeshLookups == 0);
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.movementLocalProducts
            == std::vector<std::optional<UpdateProduct>>(2, std::nullopt));
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());

    const auto restored = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp));
    REQUIRE((int) document.graph().findNode("mesh")->editorState.getProperty(
            "selectedVertexId", -1) == 0);
}

TEST_CASE("Trimesh vertex selection rebinds the committed editor state",
        "[cycle-v2][editor][trimesh][selection]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("SelectionCommand");
    node.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    mesh->destroy();
    graph.addNode(std::move(node));
    addUnrelatedInteractionState(graph);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner, document, dispatcher, presentation, resources);

    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.selectTrimeshVertexIndex("mesh", 1));
    REQUIRE((int) document.graph().findNode("mesh")->editorState.getProperty(
            "selectedVertexId", -1) == 1);
    REQUIRE(presentation.rebinds == 0);
    REQUIRE(presentation.transientRebinds == 1);
    REQUIRE(presentation.immediateRefreshes == 1);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().audioSamplesCopied == 0);
    REQUIRE(commands.selectTrimeshVertexIndex("mesh", 1));
    REQUIRE(presentation.transientRebinds == 1);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE((int) document.graph().findNode("mesh")->editorState.getProperty(
            "selectedVertexId", -1) == -1);
}

TEST_CASE("Equalizer graph drag publishes frequency and gain as one undo transaction",
        "[cycle-v2][editor][effects][equalizer]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Equalizer, "equalizer", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    const String originalGain = parameterValueForNode(
            *document.graph().findNode("equalizer"), "band3Gain");
    const String originalFrequency = parameterValueForNode(
            *document.graph().findNode("equalizer"), "band3Frequency");

    REQUIRE(commands.beginNodeParameterPairEdit(
            "equalizer", "band3Gain", "Band 3 Gain", 0.6f,
            "band3Frequency", "Band 3 Frequency", 0.4f));
    REQUIRE(commands.updateNodeParameterPairEditValues(0.75f, 0.65f));
    commands.endNodeParameterEdit();

    const Node* edited = document.graph().findNode("equalizer");
    REQUIRE(parameterValueForNode(*edited, "band3Gain") == "0.750000");
    REQUIRE(parameterValueForNode(*edited, "band3Frequency") == "0.650000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    const Node* restored = document.graph().findNode("equalizer");
    REQUIRE(parameterValueForNode(*restored, "band3Gain") == originalGain);
    REQUIRE(parameterValueForNode(*restored, "band3Frequency") == originalFrequency);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Effect discrete parameter changes are independently undoable",
        "[cycle-v2][editor][effects]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Delay, "delay", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.setNodeParameterValue("delay", "enabled", "Enabled", 0.f));
    REQUIRE(commands.setNodeParameterValue("delay", "time", "Time", 0.25f));
    REQUIRE(parameterValueForNode(*document.graph().findNode("delay"), "time") == "0.250000");
    REQUIRE(presentation.rebinds == 2);

    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("delay"), "time") == "0.5");
    REQUIRE(parameterValueForNode(*document.graph().findNode("delay"), "enabled") == "0");

    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("delay"), "enabled") == "1");
}

TEST_CASE("Envelope enable button commits one undoable edit",
        "[cycle-v2][editor][envelope][enabled][undo][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    EnvelopeEditorResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);
    NodeEditorHost host(owner, commands, presentation, resources);
    REQUIRE(host.bind(document.graph().findNode("env"), { 0, 0, 840, 684 }));
    auto* enabled = dynamic_cast<EffectEnableButton*>(
            host.component()->findChildWithID("envelopeEditor.enabled"));
    REQUIRE(enabled != nullptr);

    enabled->setToggleState(false, sendNotificationSync);

    REQUIRE(parameterValueForNode(*document.graph().findNode("env"), "enabled") == "0");
    REQUIRE(presentation.scheduledRefreshes == 1);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("env"), "enabled") == "1");
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Trimesh keeps inactive properties disabled and its Component selector available",
        "[cycle-v2][mesh-dimensions][editor][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    owner.setBounds(0, 0, 1100, 800);
    NodeGraph graph;
    auto node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh("PersistentProperties");
    node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
    mesh->destroy();
    graph.addNode(std::move(node));
    auto* selection = new DynamicObject();
    selection->setProperty("selectedVertexId", 0);
    REQUIRE(GraphNodeStateEditor().setNodeEditorState(graph, "mesh", var(selection)).succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher dispatcher(document);
    RecordingPresentation presentation;
    NullResources resources;
    TrimeshWidget widget;
    resources.activeTrimesh = &widget;
    NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
    NodeEditorHost host(owner, commands, presentation, resources);
    Rectangle<int> previousComponentBounds;

    for (const int count : { 3, 2, 1, 0 }) {
        REQUIRE(dispatcher.setMeshDimensions("mesh", count).succeeded());
        REQUIRE(commands.selectTrimeshVertexIndex("mesh", 0));
        widget.syncFromNode(*document.graph().findNode("mesh"));
        const auto* factory = NodeEditorFactoryRegistry::instance().find(NodeKind::TrilinearMesh);
        const auto bounds = factory->layoutBounds(*document.graph().findNode("mesh"), owner.getLocalBounds().toFloat());
        REQUIRE(host.bind(document.graph().findNode("mesh"), bounds.toNearestInt(), document.revision()));
        const auto parameters = widget.selectedVertexParameters();
        REQUIRE(parameters.size() == 7);
        REQUIRE(parameters[6].id == "guide.time");
        REQUIRE(parameters[6].enabled);
        REQUIRE(parameters[6].guideOnly);
        const auto dimensions = MorphDimensions::trimesh(count);
        REQUIRE(parameters[0].enabled == dimensions.active(Vertex::Time));
        REQUIRE(parameters[1].enabled == dimensions.active(Vertex::Red));
        REQUIRE(parameters[2].enabled == dimensions.active(Vertex::Blue));
        bool foundControls = false;
        for (auto* child : host.component()->getChildren()) {
            if (auto* target = child->findChildWithID("trimesh.vertex.time")) {
                foundControls = true;
                REQUIRE(target->isEnabled() == dimensions.active(Vertex::Time));
                REQUIRE(child->findChildWithID("trimesh.vertex.red")->isEnabled()
                        == dimensions.active(Vertex::Red));
                REQUIRE(child->findChildWithID("trimesh.vertex.blue")->isEnabled()
                        == dimensions.active(Vertex::Blue));
                auto* componentGuide = child->findChildWithID("trimesh.guide.time");
                REQUIRE(componentGuide != nullptr);
                REQUIRE(componentGuide->isEnabled());
                REQUIRE(componentGuide->getY() > child->findChildWithID("trimesh.vertex.curve")->getBottom());
                auto* amplitude = child->findChildWithID("trimesh.vertex.amp");
                amplitude->mouseEnter(curvePanelMouseEvent(*amplitude, { 10.f, 10.f }, {}, {}, false));
                REQUIRE(widget.propertyFeedbackId() == "vertex.amp");
                auto* tooltip = dynamic_cast<TooltipClient*>(amplitude);
                REQUIRE(tooltip != nullptr);
                REQUIRE(tooltip->getTooltip().startsWith("amp: "));
                amplitude->mouseExit(curvePanelMouseEvent(*amplitude, {}, {}, {}, false));
                REQUIRE(widget.propertyFeedbackId().isEmpty());
                if (count < 3) {
                    REQUIRE(componentGuide->getBounds() == previousComponentBounds);
                }
                previousComponentBounds = componentGuide->getBounds();
            }
        }
        REQUIRE(foundControls);
    }
}

TEST_CASE("Envelope retains disabled morph properties and an independent Component guide row",
        "[cycle-v2][mesh-dimensions][editor][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    for (const int count : { 0, 1, 2, 3 }) {
        NodeGraph graph;
        graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        REQUIRE(dispatcher.setMeshDimensions("env", count).succeeded());
        CurveEditorWidget widget(NodeKind::Envelope);
        widget.prepareExpandedPanelComponent(*document.graph().findNode("env"), { 0.f, 0.f, 900.f, 500.f });
        const auto parameters = widget.selectedVertexParameters();
        REQUIRE(parameters.size() == 7);
        REQUIRE(parameters[6].id == "guide.time");
        REQUIRE(parameters[6].enabled);
        REQUIRE(parameters[6].guideOnly);
        const MorphDimensions dimensions(count);
        for (const int index : { 0, 1, 2 }) {
            const int axis = index == 0 ? Vertex::Time : index == 1 ? Vertex::Red : Vertex::Blue;
            REQUIRE(parameters[(size_t) index].enabled == dimensions.active(axis));
            if (!dimensions.active(axis)) {
                REQUIRE_FALSE(widget.setSelectedVertexParameter(parameters[(size_t) index].id, 0.3f));
            }
        }
    }
}

TEST_CASE("Modulation profiles expose usable conditional controls and undo independent edits",
        "[cycle-v2][property-controls][modulation][regression]") {
    ScopedJuceInitialiser_GUI juce;
    for (const String prefix : { "yellow", "envelopeYellow" }) {
        for (const String source : { "constant", "midiCC" }) {
            Component owner;
            owner.setBounds(0, 0, 1100, 800);
            NodeGraph graph;
            graph.addNode(GraphNodeFactory().createNode(NodeKind::ModulationTriple, "mod", {}));
            REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, "mod", prefix + "Source", "Source", source).succeeded());
            GraphDocument document(std::move(graph));
            GraphCommandDispatcher dispatcher(document);
            RecordingPresentation presentation;
            NullResources resources;
            NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
            NodeEditorHost host(owner, commands, presentation, resources);
            const auto* factory = NodeEditorFactoryRegistry::instance().find(NodeKind::ModulationTriple);
            const auto bounds = factory->layoutBounds(*document.graph().findNode("mod"), { 0.f, 0.f, 430.f, 370.f });
            REQUIRE(bounds.getHeight() == 266.f);
            REQUIRE(host.bind(document.graph().findNode("mod"), bounds.toNearestInt()));
            const bool constant = source == "constant";
            PrecisionSlider* slider = nullptr;
            for (auto* row : host.component()->getChildren()) {
                slider = dynamic_cast<PrecisionSlider*>(row->findChildWithID(
                        "modulation." + prefix + (constant ? ".constant" : ".controller")));
                if (slider != nullptr) {
                    break;
                }
            }
            REQUIRE(slider != nullptr);
            REQUIRE(slider->isVisible());
            REQUIRE(slider->visibleTrackBounds().getWidth() >= PropertyControlMetrics::minimumUsableTrackWidth);
            REQUIRE(slider->getParentComponent()->getLocalBounds().contains(slider->getBounds()));
            const String parameter = prefix + (constant ? "Constant" : "Controller");
            const String before = parameterValueForNode(*document.graph().findNode("mod"), parameter);
            REQUIRE(slider->keyPressed(KeyPress(KeyPress::rightKey)));
            REQUIRE(parameterValueForNode(*document.graph().findNode("mod"), parameter) != before);
            const String other = prefix == "yellow" ? "envelopeYellow" : "yellow";
            REQUIRE(parameterValueForNode(*document.graph().findNode("mod"), other + "Constant") == "0.5");
            REQUIRE(document.undo());
            REQUIRE(parameterValueForNode(*document.graph().findNode("mod"), parameter) == before);
            slider->onDragStart();
            slider->setValue(constant ? 0.62 : 62.0, sendNotificationSync);
            slider->setValue(constant ? 0.74 : 74.0, sendNotificationSync);
            slider->onDragEnd();
            REQUIRE(parameterValueForNode(*document.graph().findNode("mod"), parameter) != before);
            REQUIRE(document.undo());
            REQUIRE(parameterValueForNode(*document.graph().findNode("mod"), parameter) == before);
            REQUIRE_FALSE(document.canUndo());
        }
    }
}

TEST_CASE("Envelope uses the Trimesh composition and a full width editable curve",
        "[cycle-v2][mesh-dimensions][editor][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const auto size : { Point<int>(1400, 920), Point<int>(890, 760) }) {
        NodeGraph graph;
        graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        CurveEditorWidget widget(NodeKind::Envelope);
        auto editor = createCurveNodeEditor(NodeKind::Envelope, widget);
        const auto* factory = NodeEditorFactoryRegistry::instance().find(NodeKind::Envelope);
        for (const int count : { 3, 2, 1, 0 }) {
            REQUIRE(dispatcher.setMeshDimensions("env", count).succeeded());
            const auto& node = *document.graph().findNode("env");
            const auto bounds = factory->layoutBounds(node, { 0.f, 0.f, (float) size.x, (float) size.y });
            editor->setBounds(bounds.toNearestInt());
            editor->setNode(node);
            const var state = editor->automationState();
            const auto content = editor->getLocalBounds().toFloat()
                    .withTrimmedTop(CanvasChromeMetrics::embeddedEditorHeaderHeight).reduced(12.f, 10.f);
            const MeshEditorLayout layout(content, count);
            const auto curveBounds = rectangleProperty(state, "panelBounds");
            REQUIRE(rectangleProperty(state, "panelFrameBounds") == layout.curve());
            REQUIRE(curveBounds == layout.curve().toNearestInt().toFloat().withTrimmedRight(8.f).withTrimmedBottom(8.f));
            REQUIRE(rectangleProperty(state, "controlBounds")
                    == layout.controls());
            REQUIRE(rectangleProperty(state, "morphSurfaceBounds") == layout.surface());
            const auto loop = rectangleProperty(state, "loopBounds");
            const auto sustain = rectangleProperty(state, "sustainBounds");
            REQUIRE(curveBounds.contains(loop));
            REQUIRE(curveBounds.contains(sustain));
            REQUIRE_FALSE(loop.intersects(sustain));
            REQUIRE(loop.getY() - curveBounds.getY() == Catch::Approx(8.f));
            REQUIRE(curveBounds.getRight() - sustain.getRight() == Catch::Approx(8.f));
            REQUIRE(curveBounds.getWidth() >= content.getWidth() * (count == 0 ? 0.55f : 0.99f));
            REQUIRE_FALSE(state.hasProperty("actionBarBounds"));
            REQUIRE(curveBounds.getBottom() == Catch::Approx(content.getBottom() - 8.f));
            const auto scale = rectangleProperty(state, "axisScaleBounds");
            const auto zoom = rectangleProperty(state, "rangeGroupBounds");
            REQUIRE(curveBounds.contains(scale));
            REQUIRE(curveBounds.contains(zoom));
            REQUIRE(zoom.getX() - scale.getRight() == Catch::Approx(16.f));
            REQUIRE(loop.getX() - zoom.getRight() == Catch::Approx(16.f));
            REQUIRE(zoom.getY() == Catch::Approx(loop.getY()));
            REQUIRE(scale.getY() == Catch::Approx(loop.getY()));
            const auto panel = state.getProperty("panelState", {});
            const auto horizontal = panel.getProperty("zoomControls", {}).getProperty("horizontal", {});
            REQUIRE((bool) horizontal.getProperty("visible", false));
            REQUIRE((double) horizontal.getProperty("start", {})
                    == Catch::Approx((double) panel.getProperty("zoom", {}).getProperty("x", {})));
            REQUIRE((double) horizontal.getProperty("size", {})
                    == Catch::Approx((double) panel.getProperty("zoom", {}).getProperty("w", {})));
            const auto rails = state.getProperty("vertexParameterRails", {});
            REQUIRE(rails.size() == 7);
            for (const auto& row : *rails.getArray()) {
                if (row.getProperty("id", {}) == "guide.time") {
                    continue;
                }
                const auto rail = rectangleProperty(row, "bounds");
                REQUIRE(rail.getWidth() >= PropertyControlMetrics::minimumUsableTrackWidth);
                REQUIRE(rail.getWidth() <= 220.f);
                REQUIRE_FALSE(rail.intersects(curveBounds));
            }
            if (count > 0) {
                const auto preview = rectangleProperty(state, "morphPlaneBounds");
                REQUIRE(preview.getWidth() >= (size.x > 1000 ? 200.f : 140.f));
            } else {
                REQUIRE(bounds.getHeight() < (float) size.y * 0.65f);
            }
        }
    }
}

TEST_CASE("Envelope surface coalesces gesture updates and restores authored scalar amplitudes",
        "[cycle-v2][mesh-dimensions][envelope][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    TrimeshPanelEnvironment environment;
    EnvelopeMorphSurfacePreview surface(environment, [] {});
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    auto* component = surface.component();
    component->setBounds(0, 0, 400, 300);
    const auto amplitude = [&](float value) {
        for (auto* vertex : mesh->getVerts()) {
            vertex->values[Vertex::Amp] = value;
        }
    };
    const auto prepare = [&](uint64_t revision, bool active) {
        surface.prepare(*mesh, nullptr, revision, { 0.5f, 0.5f, 0.5f }, Vertex::Time, 3, active);
        return surface.automationState();
    };
    uint64_t revision = 1;
    for (const float value : { 0.f, 0.2f, 0.5f, 0.8f, 1.f }) {
        amplitude(value);
        const auto state = prepare(revision++, false);
        REQUIRE((float) state.getProperty("centreValue", {}) == Catch::Approx(value).margin(0.0001f));
        REQUIRE((float) state.getProperty("minimum", {}) >= 0.f);
        REQUIRE((float) state.getProperty("maximum", {}) <= 1.f);
    }
    const auto before = surface.automationState();
    for (const float value : { 0.35f, 0.65f }) {
        amplitude(value);
        const auto held = prepare(revision++, true);
        REQUIRE(held.getProperty("rebuildCount", {}) == before.getProperty("rebuildCount", {}));
        REQUIRE(held.getProperty("centreValue", {}) == before.getProperty("centreValue", {}));
    }
    const auto committed = prepare(revision, false);
    REQUIRE((int64) committed.getProperty("rebuildCount", {}) == (int64) before.getProperty("rebuildCount", {}) + 1);
    REQUIRE((float) committed.getProperty("centreValue", {}) == Catch::Approx(0.65f).margin(0.0001f));
    REQUIRE(prepare(revision, false).getProperty("rebuildCount", {}) == committed.getProperty("rebuildCount", {}));
    const auto zoom = committed.getProperty("zoomControls", {});
    REQUIRE((bool) zoom.getProperty("horizontal", {}).getProperty("visible", false));
    REQUIRE((bool) zoom.getProperty("vertical", {}).getProperty("visible", false));
    REQUIRE(rectangleProperty(zoom, "viewport") == Rectangle<float>(0.f, 0.f, 388.f, 292.f));
    mesh->destroy();
}

TEST_CASE("Envelope surfaces put time on x and retain the primary morph rails and colour bar",
        "[cycle-v2][editor][envelope][morph-surface][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    const auto graph = TestSupport::authoredEnvelopeGraph();
    EnvelopeNodeModel model;
    REQUIRE(model.syncFromNode(*graph.findNode("env")));
    TrimeshPanelEnvironment environment;
    EnvelopeMorphSurfacePreview surface(environment, [] {});
    surface.component()->setBounds(0, 0, 400, 300);
    const char* ids[] { "yellow", "red", "blue" };
    for (int index = 0; index < 3; ++index) {
        CAPTURE(index);
        surface.prepare(model.getMesh(), nullptr, model.revision(),
                { 0.5f, 0.5f, 0.5f }, MorphDimensions::axes[(size_t) index], 3, false);
        const auto state = surface.automationState();
        REQUIRE(state.getProperty("timeAxis", {}).toString() == "x");
        REQUIRE(state.getProperty("primaryAxis", {}).toString() == "y");
        REQUIRE(state.getProperty("primaryViewAxis", {}).toString() == ids[index]);
        REQUIRE(state.getProperty("primaryAxisColour", {}).toString()
                == colourForMorphDimension(static_cast<MorphDimension>(index)).toString());
        const auto zoom = state.getProperty("zoomControls", {});
        const auto viewport = rectangleProperty(zoom, "viewport");
        const auto slider = rectangleProperty(zoom, "vertical");
        const auto bar = rectangleProperty(state, "primaryAxisBarBounds");
        REQUIRE(bar.getX() == Catch::Approx(slider.getRight() + 1.f));
        REQUIRE(bar.getY() == Catch::Approx(1.f));
        REQUIRE(bar.getWidth() == Catch::Approx(3.f));
        REQUIRE(bar.getHeight() == Catch::Approx(viewport.getHeight() - 2.f));
        const auto* rails = state.getProperty("vertexRails", {}).getArray();
        REQUIRE(rails != nullptr);
        REQUIRE(rails->size() >= 3);
        std::vector<float> railTimes;
        for (const auto& rail : *rails) {
            const float startX = rail.getProperty("startX", {});
            const float endX = rail.getProperty("endX", {});
            const float startY = rail.getProperty("startY", {});
            const float endY = rail.getProperty("endY", {});
            REQUIRE(startX == Catch::Approx(endX).margin(0.0001f));
            REQUIRE(std::abs(endY - startY) > 0.9f);
            railTimes.push_back(startX);
        }
        const auto extremes = std::minmax_element(railTimes.begin(), railTimes.end());
        REQUIRE(*extremes.second - *extremes.first > 0.5f);
    }
}

TEST_CASE("Envelope transposed surfaces sample and align rails across the full release extent",
        "[cycle-v2][editor][envelope][morph-surface][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    const auto graph = TestSupport::authoredEnvelopeGraph();
    EnvelopeNodeModel model;
    REQUIRE(model.syncFromNode(*graph.findNode("env")));
    for (auto* vertex : model.getMesh().getVerts()) {
        const float time = vertex->values[Vertex::Phase];
        vertex->values[Vertex::Amp] = time < 0.8f ? 0.15f : time < 1.05f ? 0.3f : 0.9f;
        vertex->values[Vertex::Curve] = 0.5f;
    }
    REQUIRE(model.synchronizeFromMesh(nullptr));
    TrimeshPanelEnvironment environment;
    EnvelopeMorphSurfacePreview surface(environment, [] {});
    surface.component()->setBounds(0, 0, 400, 300);
    const Range<float> fullTimeRange(0.f, 1.5f);
    surface.prepare(model.getMesh(), nullptr, model.revision(),
            { 0.5f, 0.5f, 0.5f }, Vertex::Red, 3, false, fullTimeRange);
    const auto state = surface.automationState();
    REQUIRE((float) state.getProperty("timeMinimum", {}) == Catch::Approx(fullTimeRange.getStart()));
    REQUIRE((float) state.getProperty("timeMaximum", {}) == Catch::Approx(fullTimeRange.getEnd()));
    const auto* samples = state.getProperty("timeSamples", {}).getArray();
    REQUIRE(samples != nullptr);
    REQUIRE(samples->size() == (int) state.getProperty("columnCount", {}));
    REQUIRE(samples->size() > 1);
    const float timeStep = fullTimeRange.getLength() / (float) (samples->size() - 1);
    REQUIRE((float) state.getProperty("sampleTimeStep", {}) == Catch::Approx(timeStep));
    REQUIRE((float) samples->getLast().getProperty("time", {}) == Catch::Approx(fullTimeRange.getEnd()));

    Rasterization::EnvelopeMorphSurfaceRasterizer reference;
    MorphPosition centreMorph { 0.5f, 0.5f, 0.5f };
    const int morphRows = state.getProperty("rowCount", {});
    REQUIRE(morphRows > 1);
    centreMorph.red.setValueDirect((float) (morphRows / 2) / (float) (morphRows - 1));
    reference.setMorphPosition(centreMorph);
    reference.renderWaveformOnly(&model.getMesh());
    REQUIRE(reference.sampler().isSampleable());
    std::vector<float> expected((size_t) samples->size());
    reference.sampler().samplePerfectly(timeStep,
            Buffer<float>(expected.data(), (int) expected.size()), fullTimeRange.getStart());
    for (int index = 0; index < samples->size(); ++index) {
        const auto& sample = samples->getReference(index);
        REQUIRE((float) sample.getProperty("time", {})
                == Catch::Approx(fullTimeRange.getStart() + (float) index * timeStep).margin(0.0001f));
        REQUIRE((float) sample.getProperty("value", {})
                == Catch::Approx(jlimit(0.f, 1.f, expected[(size_t) index] + 0.5f)).margin(0.0001f));
    }
    const int lateIndex = roundToInt(1.2f / timeStep);
    REQUIRE((float) samples->getReference(lateIndex).getProperty("time", {}) > 1.f);
    REQUIRE((float) samples->getReference(lateIndex).getProperty("value", {})
            > (float) samples->getReference(roundToInt(0.5f / timeStep)).getProperty("value", {}) + 0.4f);
    const auto* rails = state.getProperty("vertexRails", {}).getArray();
    REQUIRE(rails != nullptr);
    REQUIRE(std::any_of(rails->begin(), rails->end(), [](const var& rail) {
        return std::abs((float) rail.getProperty("startX", {}) - 1.25f) < 0.0001f
                && std::abs((float) rail.getProperty("endX", {}) - 1.25f) < 0.0001f;
    }));
}

TEST_CASE("Envelope cached surfaces retain their prepared mesh until topology can be refreshed",
        "[cycle-v2][editor][envelope][morph-surface][lifecycle][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    const auto graph = TestSupport::authoredEnvelopeGraph();
    EnvelopeNodeModel model;
    REQUIRE(model.syncFromNode(*graph.findNode("env")));
    auto& sourceMesh = model.getMesh();
    TrimeshPanelEnvironment environment;
    EnvelopeMorphSurfacePreview surface(environment, [] {});
    surface.component()->setBounds(0, 0, 400, 300);
    const Range<float> timeRange(0.f, 1.75f);
    surface.prepare(sourceMesh, nullptr, 1, { 0.5f, 0.5f, 0.5f }, Vertex::Red, 3, false, timeRange);
    const auto before = surface.automationState();
    sourceMesh.destroy();
    EnvelopeMeshState::initialiseDefault(sourceMesh);
    for (auto* cube : sourceMesh.getCubes()) {
        for (int corner = 0; corner < VertCube::numVerts; ++corner) {
            bool yellow {};
            bool red {};
            bool blue {};
            VertCube::getPoles(corner, yellow, red, blue);
            auto* vertex = cube->getVertex(corner);
            vertex->values[Vertex::Time] = yellow ? 1.f : 0.f;
            vertex->values[Vertex::Phase] *= 1.2f;
            vertex->values[Vertex::Amp] = 0.85f;
        }
    }
    const auto requireSameRows = [&](const var& actual, const char* property,
                                     std::initializer_list<const char*> fields) {
        const auto* previousRows = before.getProperty(property, {}).getArray();
        const auto* actualRows = actual.getProperty(property, {}).getArray();
        REQUIRE(previousRows != nullptr);
        REQUIRE(actualRows != nullptr);
        REQUIRE(actualRows->size() == previousRows->size());
        for (int index = 0; index < previousRows->size(); ++index) {
            for (const char* field : fields) {
                REQUIRE(actualRows->getReference(index).getProperty(field, {})
                        == previousRows->getReference(index).getProperty(field, {}));
            }
        }
    };
    InteractionComplexityDiagnostics::reset();
    surface.prepare(sourceMesh, nullptr, 2, { 0.5f, 0.5f, 0.5f }, Vertex::Red, 3, true, timeRange);
    const auto held = surface.automationState();
    REQUIRE(held.getProperty("rebuildCount", {}) == before.getProperty("rebuildCount", {}));
    requireSameRows(held, "timeSamples", { "time", "value" });
    requireSameRows(held, "vertexRails", { "startX", "startY", "endX", "endY" });
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
    surface.prepare(sourceMesh, nullptr, 2, { 0.5f, 0.5f, 0.5f }, Vertex::Red, 3, false, timeRange);
    const auto refreshed = surface.automationState();
    REQUIRE((int64) refreshed.getProperty("rebuildCount", {})
            == (int64) before.getProperty("rebuildCount", {}) + 1);
    REQUIRE((float) refreshed.getProperty("centreValue", {}) == Catch::Approx(0.85f).margin(0.0001f));
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 1);
    const auto* rails = refreshed.getProperty("vertexRails", {}).getArray();
    REQUIRE(rails != nullptr);
    REQUIRE(std::any_of(rails->begin(), rails->end(), [](const var& rail) {
        return std::abs((float) rail.getProperty("startX", {}) - 1.5f) < 0.0001f;
    }));
}

TEST_CASE("Trimesh primary morph colour bars follow the view axis along the surface bottom",
        "[cycle-v2][editor][trimesh][morph-surface][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    auto graph = TestSupport::authoredTrimeshGraph();
    TrimeshPanelBridge bridge;
    bridge.syncFromNode(*graph.findNode("mesh"), 128, 64);
    bridge.getPanel3DHostComponent()->setBounds(0, 0, 400, 300);
    const char* ids[] { "yellow", "red", "blue" };
    for (int index = 0; index < 3; ++index) {
        CAPTURE(index);
        REQUIRE(GraphNodeStateEditor().setNodeParameter(
                graph, "mesh", "primaryAxis", "Axis", ids[index]).succeeded());
        bridge.syncFromNode(*graph.findNode("mesh"), 128, 64);
        const auto& panel = bridge.getPanel3D();
        const auto bar = panel.primaryAxisBarBounds();
        const auto bounds = panel.getLocalBounds().toFloat();
        REQUIRE_FALSE(panel.isPrimaryAxisVertical());
        REQUIRE(panel.primaryAxisColour()
                == colourForMorphDimension(static_cast<MorphDimension>(index)));
        REQUIRE(bar.getX() == Catch::Approx(1.f));
        REQUIRE(bar.getBottom() == Catch::Approx(bounds.getHeight() - 1.f));
        REQUIRE(bar.getWidth() == Catch::Approx(bounds.getWidth() - 2.f));
        REQUIRE(bar.getHeight() == Catch::Approx(3.f));
    }
}

TEST_CASE("Morph keyboard hints disappear only when their slider disappears",
        "[cycle-v2][mesh-dimensions][editor][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const auto kind : { NodeKind::Envelope, NodeKind::TrilinearMesh }) {
        Component owner;
        owner.setSize(1000, 820);
        NodeGraph graph;
        graph.addNode(GraphNodeFactory().createNode(kind, "subject", {}));
        REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, "subject", "dimensionCount", "Dimensions", "3").succeeded());
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        RecordingPresentation presentation;
        EnvelopeEditorResources resources;
        TrimeshWidget mesh;
        resources.activeTrimesh = &mesh;
        NodeEditorCommandService service(owner, document, commands, presentation, resources);
        NodeEditorHost host(owner, service, presentation, resources);
        const auto bind = [&] {
            REQUIRE(host.bind(document.graph().findNode("subject"), owner.getLocalBounds(), document.revision()));
        };
        bind();
        const auto focused = [&]() {
            if (kind == NodeKind::Envelope) {
                return dynamic_cast<EnvelopeEditorComponent*>(host.component())->focusedMorphAxis();
            }
            return dynamic_cast<TrimeshExpandedEditorComponent*>(host.component())->focusedMorphAxis();
        };
        const auto hint = [&]() -> Component* {
            if (kind == NodeKind::Envelope) {
                return host.component()->findChildWithID("envelopeEditor.morph.shortcuts");
            }
            for (auto* child : host.component()->getChildren()) {
                if (auto* found = child->findChildWithID("trimesh.morph.shortcuts")) {
                    return found;
                }
            }
            return nullptr;
        };
        REQUIRE(host.component()->keyPressed(KeyPress('r')));
        REQUIRE(commands.setMeshDimensions("subject", 2).succeeded());
        bind();
        REQUIRE(focused() == "red");
        REQUIRE(hint() != nullptr);
        REQUIRE(hint()->isVisible());
        REQUIRE(commands.setMeshDimensions("subject", 1).succeeded());
        bind();
        REQUIRE(focused().isEmpty());
        REQUIRE_FALSE(hint()->isVisible());
        REQUIRE(host.component()->keyPressed(KeyPress('y')));
        REQUIRE(commands.setMeshDimensions("subject", 3).succeeded());
        bind();
        REQUIRE(focused() == "yellow");
        REQUIRE(hint()->isVisible());
        REQUIRE(commands.setMeshDimensions("subject", 0).succeeded());
        bind();
        REQUIRE(focused().isEmpty());
        REQUIRE_FALSE(hint()->isVisible());
    }
}

TEST_CASE("Envelope morph gestures survive live preview injection release and settled rebinds",
        "[cycle-v2][editor][envelope][morph-preview][complexity][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const int count : { 2, 3 }) {
        CAPTURE(count);
        Component owner;
        owner.setSize(1100, 800);
        owner.addToDesktop(ComponentPeer::windowIsTemporary);
        owner.setVisible(true);
        auto graph = TestSupport::authoredEnvelopeGraph(count);
        GraphNodeFactory factory;
        GraphNodeStateEditor stateEditor;
        const char* axes[] { "yellow", "red", "blue" };
        const int controllers[] { 71, 73, 74 };
        for (int axis = 0; axis < 3; ++axis) {
            if (!MorphDimensions(count).active(MorphDimensions::axes[(size_t) axis])) {
                continue;
            }
            const String sourceId = "cc" + String(controllers[axis]);
            graph.addNode(factory.createNode(NodeKind::ModulationSource, sourceId, {}));
            REQUIRE(stateEditor.setNodeParameter(graph, sourceId, "source", "Source", "midiCC").succeeded());
            REQUIRE(stateEditor.setNodeParameter(graph, sourceId, "controller", "Controller", String(controllers[axis])).succeeded());
            graph.addEdge({ sourceId, "value", "env", axes[axis],
                    PortDomain::ControlSignal, ConnectionKind::Signal });
        }
        graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        GraphEditor graphEditor;
        REQUIRE(graphEditor.connect(graph, { "mesh", "out", false }, { "multiply", "left", true }).succeeded());
        REQUIRE(graphEditor.connect(graph, { "env", "env", false }, { "multiply", "right", true }).succeeded());
        REQUIRE(graphEditor.connect(graph, { "multiply", "out", false }, { "out", "time", true }).succeeded());
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        RecordingPresentation presentation;
        PreviewEnvelopeEditorResources resources;
        NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
        NodePreviewResources previews(commands);
        GraphPresentationModel previewControls;
        previewControls.stagePreviewControllerValue(71, 32);
        previewControls.stagePreviewControllerValue(73, 48);
        previewControls.stagePreviewControllerValue(74, 96);
        REQUIRE(previewControls.refresh(document.graph(), document.revision()));
        const auto injected = previewControls.previewMorphForNode(*document.graph().findNode("env"));
        REQUIRE(injected.has_value());
        REQUIRE((float) injected->red == Catch::Approx(48.f / 127.f));
        REQUIRE((float) injected->blue == Catch::Approx(96.f / 127.f));
        previews.setDurableGraph(&document.graph());
        previews.setPreviewControls(previewControls);
        resources.previews = &previews;
        previews.syncCurveEditorWidget(*document.graph().findNode("env"));
        NodeEditorHost host(owner, commands, presentation, resources);
        const auto bind = [&] {
            REQUIRE(host.bind(document.graph().findNode("env"), owner.getLocalBounds(), document.revision()));
        };
        bind();
        auto* editor = dynamic_cast<EnvelopeEditorComponent*>(host.component());
        REQUIRE(editor != nullptr);
        auto& widget = previews.curveEditorWidget(*document.graph().findNode("env"));
        const auto effective = widget.effectiveMorphPosition();
        REQUIRE(effective.has_value());
        const auto initial = std::array<float, 3> { (float) effective->time, (float) effective->red, (float) effective->blue };
        const auto durableBefore = std::array<float, 3> { 0.4f, 0.3f, 0.7f };
        const uint64_t revision = document.revision();
        const uint64_t graphRevision = document.graph().getRevision();
        const auto model = document.graph().findNode("env")->model;
        int publications {};
        document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
        const MorphSpaceLayout space(rectangleProperty(editor->automationState(), "morphPlaneBounds"), MorphDimensions(count));
        const auto source = count == 3 ? space.cubeFacePoint(0, 0.5f, 0.5f)
                : space.project(initial[1], initial[2], initial[0]);
        REQUIRE(editor->getComponentAt(source.roundToInt()) == editor);
        Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
        Graphics graphics(image);
        const auto coordinates = [&](std::array<float, 3> expected, bool checkControls) {
            const auto state = editor->automationState();
            const char* displayed[] { "displayYellowMorph", "displayRedMorph", "displayBlueMorph" };
            const char* resolved[] { "effectiveYellowMorph", "effectiveRedMorph", "effectiveBlueMorph" };
            for (int axis = 0; axis < 3; ++axis) {
                REQUIRE((float) state.getProperty(displayed[axis], {})
                        == Catch::Approx(expected[(size_t) axis]).margin(0.001f));
                REQUIRE((float) state.getProperty(resolved[axis], {})
                        == Catch::Approx(expected[(size_t) axis]).margin(0.001f));
            }
            if (checkControls) {
                REQUIRE((float) state.getProperty("redMorph", {}) == Catch::Approx(expected[1]).margin(0.001f));
                REQUIRE((float) state.getProperty("blueMorph", {}) == Catch::Approx(expected[2]).margin(0.001f));
            }
            const auto cursor = state.getProperty("morphCursor", {});
            const auto expectedCursor = space.project(expected[1], expected[2], expected[0]);
            REQUIRE((float) cursor.getProperty("x", {}) == Catch::Approx(expectedCursor.x).margin(0.01f));
            REQUIRE((float) cursor.getProperty("y", {}) == Catch::Approx(expectedCursor.y).margin(0.01f));
        };
        editor->mouseDown(curvePanelMouseEvent(*editor, source, ModifierKeys::leftButtonModifier, source, false));
        Point<float> destination;
        auto final = initial;
        for (const Point<float> target : { Point<float>(0.2f, 0.8f), Point<float>(0.7f, 0.25f) }) {
            final[1] = target.x;
            final[2] = target.y;
            destination = space.project(final[1], final[2], final[0]);
            if (count == 3) {
                destination = source + destination - space.project(initial[1], initial[2], initial[0]);
            }
            InteractionComplexityDiagnostics::reset();
            editor->mouseDrag(curvePanelMouseEvent(*editor, destination, ModifierKeys::leftButtonModifier, source, true));
            previews.syncCurveEditorWidget(*document.graph().findNode("env"));
            bind();
            editor->paint(graphics);
            coordinates(final, true);
            REQUIRE(publications == 0);
            REQUIRE(document.revision() == revision);
            REQUIRE(document.graph().getRevision() == graphRevision);
            REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
            REQUIRE(document.graph().findNode("env")->model == model);
            REQUIRE_FALSE(document.canUndo());
            const auto operations = InteractionComplexityDiagnostics::counts();
            REQUIRE(operations.graphCopies == 0);
            REQUIRE(operations.meshCopies == 0);
            REQUIRE(operations.modelSerializations == 0);
        }
        editor->mouseUp(curvePanelMouseEvent(*editor, destination, {}, source, true));
        editor->paint(graphics);
        coordinates(final, true);
        REQUIRE(publications == 1);
        REQUIRE(document.revision() == revision + 1);
        for (int settled = 0; settled < 2; ++settled) {
            REQUIRE(previewControls.refresh(document.graph(), document.revision(), document.lastChange()));
            previews.syncCurveEditorWidget(*document.graph().findNode("env"));
            bind();
            editor->paint(graphics);
            coordinates(final, true);
            REQUIRE(publications == 1);
        }
        const auto committedRevision = document.revision();
        const auto committedGraphRevision = document.graph().getRevision();
        const auto committedModel = document.graph().findNode("env")->model;
        // Exercise the round trip while the prior manual value still owns Blue.
        for (const int controllerChangesWhileHeld : { 0, 2, 1 }) {
            CAPTURE(controllerChangesWhileHeld);
            const auto cancelEffective = widget.effectiveMorphPosition();
            REQUIRE(cancelEffective.has_value());
            const auto cancelInitial = std::array<float, 3> {
                    (float) cancelEffective->time, (float) cancelEffective->red, (float) cancelEffective->blue
            };
            const auto cancelSource = count == 3 ? space.cubeFacePoint(0, 0.5f, 0.5f)
                    : space.project(cancelInitial[1], cancelInitial[2], cancelInitial[0]);
            auto pending = cancelInitial;
            pending[1] = 0.1f;
            pending[2] = 0.9f;
            auto cancelDestination = space.project(pending[1], pending[2], pending[0]);
            if (count == 3) {
                cancelDestination = cancelSource + cancelDestination
                        - space.project(cancelInitial[1], cancelInitial[2], cancelInitial[0]);
            }
            editor->mouseDown(curvePanelMouseEvent(
                    *editor, cancelSource, ModifierKeys::leftButtonModifier, cancelSource, false));
            editor->mouseDrag(curvePanelMouseEvent(
                    *editor, cancelDestination, ModifierKeys::leftButtonModifier, cancelSource, true));
            previews.syncCurveEditorWidget(*document.graph().findNode("env"));
            editor->paint(graphics);
            coordinates(pending, true);
            auto reopened = cancelInitial;
            if (controllerChangesWhileHeld > 0) {
                const int previousController = previewControls.previewControllerValue(74);
                REQUIRE(previewControls.refreshPreviewControllerValue(document.graph(), document.revision(), 74, 59));
                previews.refreshControllerPreview(74);
                reopened[2] = 59.f / 127.f;
                if (controllerChangesWhileHeld == 2) {
                    REQUIRE(previewControls.refreshPreviewControllerValue(
                            document.graph(), document.revision(), 74, previousController));
                    previews.refreshControllerPreview(74);
                    reopened[2] = (float) previousController / 127.f;
                }
            }
            REQUIRE(publications == 1);
            host.close();
            REQUIRE_FALSE(host.hasEditor());
            REQUIRE_FALSE(dispatcher.hasTransientEdit());
            REQUIRE_FALSE((bool) widget.automationState().getProperty("surfaceGestureActive", true));
            REQUIRE(document.revision() == committedRevision);
            REQUIRE(document.graph().getRevision() == committedGraphRevision);
            REQUIRE(dispatcher.editingGraph().getRevision() == committedGraphRevision);
            REQUIRE(document.graph().findNode("env")->model == committedModel);
            REQUIRE(publications == 1);
            REQUIRE(document.canUndo());
            REQUIRE_FALSE(document.canRedo());
            previews.syncCurveEditorWidget(*document.graph().findNode("env"));
            bind();
            editor = dynamic_cast<EnvelopeEditorComponent*>(host.component());
            REQUIRE(editor != nullptr);
            editor->paint(graphics);
            coordinates(reopened, false);
            REQUIRE((float) editor->automationState().getProperty("redMorph", {}) == Catch::Approx(final[1]));
            REQUIRE((float) editor->automationState().getProperty("blueMorph", {}) == Catch::Approx(final[2]));
            REQUIRE_FALSE((bool) widget.automationState().getProperty("surfaceGestureActive", true));
            REQUIRE(document.revision() == committedRevision);
            REQUIRE(publications == 1);
        }
        REQUIRE(previewControls.refreshPreviewControllerValue(document.graph(), document.revision(), 74, 83));
        previews.refreshControllerPreview(74);
        editor->paint(graphics);
        auto controlled = final;
        controlled[2] = 83.f / 127.f;
        coordinates(controlled, false);
        REQUIRE(publications == 1);
        REQUIRE(document.revision() == revision + 1);
        const NodeParameterMap committed(*document.graph().findNode("env"));
        REQUIRE(committed.floatValue("yellow") == Catch::Approx(durableBefore[0]));
        REQUIRE(committed.floatValue("red") == Catch::Approx(final[1]).margin(0.001f));
        REQUIRE(committed.floatValue("blue") == Catch::Approx(final[2]).margin(0.001f));
        REQUIRE(dispatcher.setNodeParameter("env", "red", "Red", "0.45").changed);
        REQUIRE(document.graph().findNode("env")->model == committedModel);
        previews.syncCurveEditorWidget(*document.graph().findNode("env"));
        const auto reconciled = widget.effectiveMorphPosition();
        REQUIRE(reconciled.has_value());
        REQUIRE((float) reconciled->red == Catch::Approx(48.f / 127.f));
        REQUIRE((float) reconciled->blue == Catch::Approx(controlled[2]));
        bind();
        editor->paint(graphics);
        auto authoredChange = controlled;
        authoredChange[1] = 48.f / 127.f;
        coordinates(authoredChange, false);
        REQUIRE((float) editor->automationState().getProperty("redMorph", {}) == Catch::Approx(0.45f));
        REQUIRE(document.graph().findNode("env")->model == committedModel);
        REQUIRE(publications == 2);
        REQUIRE(document.undo());
        REQUIRE(NodeParameterMap(*document.graph().findNode("env")).floatValue("red") == Catch::Approx(final[1]));
        REQUIRE(document.canUndo());
        REQUIRE(document.undo());
        const NodeParameterMap undone(*document.graph().findNode("env"));
        for (int axis = 0; axis < 3; ++axis) {
            REQUIRE(undone.floatValue(axes[axis]) == Catch::Approx(durableBefore[(size_t) axis]));
        }
        REQUIRE_FALSE(document.canUndo());
    }
}

TEST_CASE("Reduced Envelope diagram gestures follow visible axes and preserve inactive positions",
        "[cycle-v2][editor][envelope][axis-lock][morph-cursor][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    for (const int count : { 1, 2 }) {
        Component owner;
        owner.setBounds(0, 0, 1100, 800);
        NodeGraph graph = TestSupport::authoredEnvelopeGraph(count);
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        RecordingPresentation presentation;
        EnvelopeEditorResources resources;
        resources.curveEditorWidget(*document.graph().findNode("env"))
                ->syncFromNode(*document.graph().findNode("env"));
        NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
        NodeEditorHost host(owner, commands, presentation, resources);
        REQUIRE(host.bind(document.graph().findNode("env"), owner.getLocalBounds(), document.revision()));
        auto* editor = dynamic_cast<EnvelopeEditorComponent*>(host.component());
        REQUIRE(editor != nullptr);
        const auto values = [&] {
            const NodeParameterMap parameters(*dispatcher.editingGraph().findNode("env"));
            return std::array<float, 3> { parameters.floatValue("yellow", 0.5f),
                    parameters.floatValue("red", 0.5f), parameters.floatValue("blue", 0.5f) };
        };
        const auto before = values();
        const auto documentRevision = document.revision();
        const auto graphRevision = document.graph().getRevision();
        const auto model = document.graph().findNode("env")->model;
        const auto input = rectangleProperty(editor->automationState(), "morphInputBounds");
        REQUIRE_FALSE(input.isEmpty());
        const auto source = input.getCentre();
        const MorphSpaceLayout space(
                rectangleProperty(editor->automationState(), "morphPlaneBounds"), MorphDimensions(count));
        Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
        Graphics graphics(image);
        editor->mouseDown(curvePanelMouseEvent(*editor, source,
                ModifierKeys::leftButtonModifier, source, false));
        for (const float value : { 0.f, 1.f }) {
            const auto destination = value == 0.f ? input.getBottomLeft() : input.getTopRight();
            InteractionComplexityDiagnostics::reset();
            editor->mouseDrag(curvePanelMouseEvent(*editor, destination,
                    ModifierKeys::leftButtonModifier, source, true));
            editor->paint(graphics);
            const auto held = editor->automationState();
            const float yellow = held.getProperty("yellowMorph", {});
            const float red = held.getProperty("redMorph", {});
            const float blue = held.getProperty("blueMorph", {});
            REQUIRE((count == 1 ? yellow : red) == Catch::Approx(value));
            REQUIRE((float) held.getProperty("displayYellowMorph", {}) == Catch::Approx(yellow));
            REQUIRE((float) held.getProperty("displayRedMorph", {}) == Catch::Approx(red));
            REQUIRE((float) held.getProperty("displayBlueMorph", {}) == Catch::Approx(blue));
            if (count == 2) {
                REQUIRE(blue == Catch::Approx(value));
                REQUIRE(yellow == Catch::Approx(before[0]));
            } else {
                REQUIRE(red == Catch::Approx(before[1]));
                REQUIRE(blue == Catch::Approx(before[2]));
            }
            const auto expectedCursor = space.project(red, blue, yellow);
            const auto cursor = held.getProperty("morphCursor", {});
            REQUIRE((float) cursor.getProperty("x", {}) == Catch::Approx(expectedCursor.x).margin(0.01f));
            REQUIRE((float) cursor.getProperty("y", {}) == Catch::Approx(expectedCursor.y).margin(0.01f));
            REQUIRE(document.revision() == documentRevision);
            REQUIRE(document.graph().getRevision() == graphRevision);
            REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
            REQUIRE(document.graph().findNode("env")->model == model);
            REQUIRE_FALSE(document.canUndo());
            const auto operations = InteractionComplexityDiagnostics::counts();
            REQUIRE(operations.graphCopies == 0);
            REQUIRE(operations.meshCopies == 0);
            REQUIRE(operations.modelSerializations == 0);
        }
        editor->mouseUp(curvePanelMouseEvent(*editor, input.getTopRight(), {}, source, true));
        const auto after = values();
        if (count == 1) {
            REQUIRE(after[0] == Catch::Approx(1.f));
            REQUIRE(after[1] == Catch::Approx(before[1]));
            REQUIRE(after[2] == Catch::Approx(before[2]));
        } else {
            REQUIRE(after[0] == Catch::Approx(before[0]));
            REQUIRE(after[1] == Catch::Approx(1.f));
            REQUIRE(after[2] == Catch::Approx(1.f));
        }
        REQUIRE(document.undo());
        REQUIRE(values() == before);
    }
}

TEST_CASE("Reduced Trimesh diagrams route pointer gestures to live local morph cursors",
        "[cycle-v2][editor][trimesh][morph-cursor][regression][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const int count : { 1, 2 }) {
        CAPTURE(count);
        Component owner;
        owner.setSize(1100, 800);
        owner.addToDesktop(ComponentPeer::windowIsTemporary);
        owner.setVisible(true);
        auto graph = TestSupport::authoredTrimeshGraph(count);
        const char* ids[] { "yellow", "red", "blue" };
        const char* values[] { "0.4", "0.3", "0.7" };
        for (int axis = 0; axis < 3; ++axis) {
            REQUIRE(GraphNodeStateEditor().setNodeParameter(
                    graph, "mesh", ids[axis], ids[axis], values[axis]).succeeded());
        }
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        RecordingPresentation presentation;
        NullResources resources;
        TrimeshWidget widget;
        resources.activeTrimesh = &widget;
        NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
        NodeEditorHost host(owner, commands, presentation, resources);
        REQUIRE(host.bind(document.graph().findNode("mesh"), owner.getLocalBounds(), document.revision()));
        auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
        REQUIRE(editor != nullptr);
        const auto content = editor->getLocalBounds().toFloat().withTrimmedTop(34.f).reduced(10.f, 8.f);
        const auto regions = TrimeshWidget::expandedControlHitRegions(content, true, "gain", count);
        const auto diagram = std::find_if(regions.begin(), regions.end(), [](const auto& region) {
            return region.kind == TrimeshExpandedHitRegionKind::MorphSpace;
        });
        REQUIRE(diagram != regions.end());
        const MorphSpaceLayout space(widget.morphDiagramBounds(content), MorphDimensions::trimesh(count));
        const auto source = space.project(0.5f, 0.7f, 0.5f);
        REQUIRE(diagram->bounds.contains(source));
        auto* input = editor->getComponentAt(source.roundToInt());
        REQUIRE(input != nullptr);
        REQUIRE(input->getComponentID() == "trimesh.morphSpace");
        const auto inputSource = input->getLocalPoint(editor, source);
        const auto before = std::array<float, 3> { 0.4f, 0.3f, 0.7f };
        const auto documentRevision = document.revision();
        const auto graphRevision = document.graph().getRevision();
        const auto model = document.graph().findNode("mesh")->model;
        Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
        Graphics graphics(image);
        editor->paint(graphics);
        const auto surfaceRebuilds = widget.panelRenderStatsForAutomation().surfaceRebuildCount;
        int publications {};
        document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
        input->mouseDown(curvePanelMouseEvent(
                *input, inputSource, ModifierKeys::leftButtonModifier, inputSource, false));
        Point<float> destination;
        for (const float yellow : { 0.2f, 0.8f }) {
            const float red = count == 2 ? 1.f - yellow : before[1];
            destination = space.project(red, before[2], yellow);
            InteractionComplexityDiagnostics::reset();
            input->mouseDrag(curvePanelMouseEvent(
                    *input, input->getLocalPoint(editor, destination),
                    ModifierKeys::leftButtonModifier, inputSource, true));
            editor->paint(graphics);
            const auto morph = widget.editorMorphPosition();
            const auto local = std::array<float, 3> { (float) morph.time, (float) morph.red, (float) morph.blue };
            REQUIRE(local[0] == Catch::Approx(yellow).margin(0.001f));
            REQUIRE(local[1] == Catch::Approx(red).margin(0.001f));
            REQUIRE(local[2] == Catch::Approx(before[2]).margin(0.001f));
            DynamicObject state;
            host.appendAutomationState(state);
            const auto cursor = state.getProperty("morphCursor");
            REQUIRE((float) cursor.getProperty("x", {}) == Catch::Approx(destination.x).margin(0.01f));
            REQUIRE((float) cursor.getProperty("y", {}) == Catch::Approx(destination.y).margin(0.01f));
            REQUIRE(document.revision() == documentRevision);
            REQUIRE(document.graph().getRevision() == graphRevision);
            REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
            REQUIRE(document.graph().findNode("mesh")->model == model);
            REQUIRE(widget.panelRenderStatsForAutomation().surfaceRebuildCount == surfaceRebuilds);
            REQUIRE(publications == 0);
            REQUIRE_FALSE(document.canUndo());
            for (int axis = 0; axis < 3; ++axis) {
                REQUIRE(NodeParameterMap(*dispatcher.editingGraph().findNode("mesh"))
                        .floatValue(ids[axis]) == Catch::Approx(before[(size_t) axis]));
            }
            const auto operations = InteractionComplexityDiagnostics::counts();
            REQUIRE(operations.graphCopies == 0);
            REQUIRE(operations.meshCopies == 0);
            REQUIRE(operations.modelSerializations == 0);
        }
        input->mouseUp(curvePanelMouseEvent(
                *input, input->getLocalPoint(editor, destination), {}, inputSource, true));
        const auto durable = [&] {
            const NodeParameterMap parameters(*document.graph().findNode("mesh"));
            return std::array<float, 3> { parameters.floatValue("yellow"),
                    parameters.floatValue("red"), parameters.floatValue("blue") };
        };
        REQUIRE(document.graph().findNode("mesh")->model == model);
        REQUIRE_FALSE(dispatcher.hasTransientEdit());
        if (count == 1) {
            REQUIRE(durable() == before);
            REQUIRE((float) widget.editorMorphPosition().time == Catch::Approx(0.8f));
            REQUIRE(document.revision() == documentRevision);
            REQUIRE(document.graph().getRevision() == graphRevision);
            REQUIRE(publications == 0);
            REQUIRE_FALSE(document.canUndo());
            REQUIRE_FALSE(document.isDirty());
        } else {
            REQUIRE(durable()[0] == Catch::Approx(0.8f));
            REQUIRE(durable()[1] == Catch::Approx(0.2f));
            REQUIRE(durable()[2] == Catch::Approx(before[2]));
            REQUIRE(document.revision() == documentRevision + 1);
            REQUIRE(publications == 1);
            REQUIRE(document.undo());
            REQUIRE(durable() == before);
            REQUIRE_FALSE(document.canUndo());
        }
    }
}

TEST_CASE("Hosted Trimesh morph rails follow pointer updates without publishing movement",
        "[cycle-v2][editor][trimesh][morph-rail][regression][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    const char* ids[] { "yellow", "red", "blue" };
    for (const bool unrelatedState : { false, true }) {
        for (int primary = 0; primary < 3; ++primary) {
            for (int axis = 0; axis < 3; ++axis) {
                if (primary != 0 && axis != primary) {
                    continue;
                }
                CAPTURE(unrelatedState, primary, axis);
                Component owner;
                owner.setSize(1100, 800);
                owner.addToDesktop(ComponentPeer::windowIsTemporary);
                owner.setVisible(true);
                NodeGraph graph;
                auto node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
                auto mesh = TrimeshMeshFactory::createDefaultMesh("PointerMorphRail");
                node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
                mesh->destroy();
                graph.addNode(std::move(node));
                graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
                graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "out", {}));
                graph.addEdge({ "voice", "context", "mesh", "context",
                        PortDomain::DomainContext, ConnectionKind::Signal });
                graph.addEdge({ "mesh", "out", "out", "time",
                        PortDomain::TimeSignal, ConnectionKind::Signal });
                GraphNodeStateEditor stateEditor;
                REQUIRE(stateEditor.setNodeParameter(
                        graph, "mesh", "dimensionCount", "Dimensions", "3").succeeded());
                REQUIRE(stateEditor.setNodeParameter(
                        graph, "mesh", "primaryAxis", "Axis", ids[primary]).succeeded());
                for (const char* id : ids) {
                    REQUIRE(stateEditor.setNodeParameter(graph, "mesh", id, id, "0.5").succeeded());
                }
                if (unrelatedState) {
                    addUnrelatedInteractionState(graph);
                }
                GraphDocument document(std::move(graph));
                GraphCommandDispatcher dispatcher(document);
                RecordingPresentation presentation;
                presentation.refreshMode = ProbeRefreshMode::LiveLatest;
                PreviewTrimeshEditorResources resources;
                NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
                NodePreviewResources previews(commands);
                GraphPresentationModel previewControls;
                REQUIRE(previewControls.refresh(document.graph(), document.revision()));
                REQUIRE(previewControls.previewMorphForNode(*document.graph().findNode("mesh")).has_value());
                previews.setDurableGraph(&document.graph());
                previews.setPreviewControls(previewControls);
                resources.previews = &previews;
                resources.graph = &document.graph();
                NodeEditorHost host(owner, commands, presentation, resources);
                REQUIRE(host.bind(document.graph().findNode("mesh"), owner.getLocalBounds(), document.revision()));
                auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
                REQUIRE(editor != nullptr);
                auto& widget = *previews.findTrimeshWidget("mesh");
                const auto content = editor->getLocalBounds().toFloat().withTrimmedTop(34.f).reduced(10.f, 8.f);
                const auto regions = TrimeshWidget::expandedControlHitRegions(content, true, "gain", 3);
                const auto region = std::find_if(regions.begin(), regions.end(), [&](const auto& value) {
                    return value.kind == TrimeshExpandedHitRegionKind::MorphControl
                            && value.parameterId == ids[axis];
                });
                REQUIRE(region != regions.end());
                const auto rail = region->bounds.reduced(8.f, 12.f);
                const auto source = rail.getCentre();
                auto* input = editor->getComponentAt(source.roundToInt());
                REQUIRE(input != nullptr);
                REQUIRE(input->getComponentID() == "trimesh.morph." + String(ids[axis]));
                const auto inputSource = input->getLocalPoint(editor, source);
                Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
                Graphics graphics(image);
                editor->paint(graphics);
                const auto before = trimeshEditorMorphPosition(host);
                const auto model = document.graph().findNode("mesh")->model;
                const uint64_t revision = document.revision();
                const uint64_t graphRevision = document.graph().getRevision();
                const uint64_t surfaceRebuilds = widget.panelRenderStatsForAutomation().surfaceRebuildCount;
                int publications {};
                document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
                input->mouseDown(curvePanelMouseEvent(
                        *input, inputSource, ModifierKeys::leftButtonModifier, inputSource, false));
                Point<float> destination;
                for (const float value : { 0.2f, 0.8f }) {
                    destination = { rail.getX() + value * rail.getWidth(), source.y };
                    InteractionComplexityDiagnostics::reset();
                    input->mouseDrag(curvePanelMouseEvent(
                            *input, input->getLocalPoint(editor, destination),
                            ModifierKeys::leftButtonModifier, inputSource, true));
                    editor->paint(graphics);
                    const auto counts = InteractionComplexityDiagnostics::counts();
                    REQUIRE(counts.graphCopies == 0);
                    REQUIRE(counts.audioSamplesCopied == 0);
                    REQUIRE(counts.meshCopies == 0);
                    REQUIRE(counts.modelSerializations == 0);
                    REQUIRE(counts.validationNodeVisits == 0);
                    REQUIRE(counts.validationEdgeVisits == 0);
                    REQUIRE(publications == 0);
                    REQUIRE(document.revision() == revision);
                    REQUIRE(document.graph().getRevision() == graphRevision);
                    REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
                    REQUIRE(document.graph().findNode("mesh")->model == model);
                    REQUIRE_FALSE(document.canUndo());
                    const auto actual = trimeshEditorMorphPosition(host);
                    CHECK(actual[(size_t) axis] == Catch::Approx(value).margin(0.025f));
                    for (int untouched = 0; untouched < 3; ++untouched) {
                        REQUIRE(NodeParameterMap(*dispatcher.editingGraph().findNode("mesh"))
                                .floatValue(ids[untouched]) == Catch::Approx(0.5f));
                        if (untouched != axis) {
                            REQUIRE(actual[(size_t) untouched] == before[(size_t) untouched]);
                        }
                    }
                    if (axis == primary) {
                        REQUIRE(widget.panelRenderStatsForAutomation().surfaceRebuildCount == surfaceRebuilds);
                    }
                }
                const auto moved = trimeshEditorMorphPosition(host);
                const auto movedRebuilds = widget.panelRenderStatsForAutomation().surfaceRebuildCount;
                InteractionComplexityDiagnostics::reset();
                input->mouseUp(curvePanelMouseEvent(
                        *input, input->getLocalPoint(editor, destination), {}, inputSource, true));
                editor->paint(graphics);
                REQUIRE(trimeshEditorMorphPosition(host) == moved);
                REQUIRE(document.graph().findNode("mesh")->model == model);
                REQUIRE(InteractionComplexityDiagnostics::counts().modelSerializations == 0);
                previews.trimeshWidget(*document.graph().findNode("mesh"), &document.graph());
                REQUIRE(host.bind(document.graph().findNode("mesh"), owner.getLocalBounds(), document.revision()));
                editor->paint(graphics);
                REQUIRE(trimeshEditorMorphPosition(host) == moved);
                if (axis == primary) {
                    REQUIRE(publications == 0);
                    REQUIRE(document.revision() == revision);
                    REQUIRE(document.graph().getRevision() == graphRevision);
                    REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
                    REQUIRE_FALSE(document.canUndo());
                    REQUIRE_FALSE(document.isDirty());
                    REQUIRE(widget.panelRenderStatsForAutomation().surfaceRebuildCount == surfaceRebuilds);
                    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
                } else {
                    REQUIRE(publications == 1);
                    REQUIRE(document.revision() == revision + 1);
                    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                            .floatValue(ids[axis]) == Catch::Approx(moved[(size_t) axis]).margin(0.001f));
                    REQUIRE(widget.panelRenderStatsForAutomation().surfaceRebuildCount == movedRebuilds);
                    if (axis == 2) {
                        REQUIRE(previewControls.refreshPreviewControllerValue(
                                document.graph(), document.revision(), 1, 32));
                        previews.refreshControllerPreview(1);
                        editor->paint(graphics);
                        const auto controlled = trimeshEditorMorphPosition(host);
                        REQUIRE(controlled[0] == moved[0]);
                        REQUIRE(controlled[1] == moved[1]);
                        REQUIRE(controlled[2] == Catch::Approx(32.f / 127.f));
                        REQUIRE(document.revision() == revision + 1);
                        REQUIRE(publications == 1);
                    }
                    REQUIRE(document.undo());
                    REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                            .floatValue(ids[axis]) == Catch::Approx(0.5f));
                    REQUIRE_FALSE(document.canUndo());
                }
            }
        }
    }
}

TEST_CASE("Trimesh rail close and document reset preserve the correct local morph state",
        "[cycle-v2][editor][trimesh][morph-lifecycle][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const String action : { "close-secondary", "close-primary", "reset-document" }) {
        CAPTURE(action);
        Component owner;
        owner.setSize(1100, 800);
        owner.addToDesktop(ComponentPeer::windowIsTemporary);
        owner.setVisible(true);
        NodeGraph graph = TestSupport::authoredTrimeshGraph();
        graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
        graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "out", {}));
        graph.addEdge({ "voice", "context", "mesh", "context",
                PortDomain::DomainContext, ConnectionKind::Signal });
        graph.addEdge({ "mesh", "out", "out", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal });
        const char* ids[] { "yellow", "red", "blue" };
        for (const char* id : ids) {
            REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, "mesh", id, id, "0.5").succeeded());
        }
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher dispatcher(document);
        RecordingPresentation presentation;
        PreviewTrimeshEditorResources resources;
        NodeEditorCommandService commands(owner, document, dispatcher, presentation, resources);
        NodePreviewResources previews(commands);
        GraphPresentationModel previewControls;
        REQUIRE(previewControls.refresh(document.graph(), document.revision()));
        REQUIRE(previewControls.previewMorphForNode(*document.graph().findNode("mesh")).has_value());
        previews.setDurableGraph(&document.graph());
        previews.setPreviewControls(previewControls);
        resources.previews = &previews;
        resources.graph = &document.graph();
        NodeEditorHost host(owner, commands, presentation, resources);
        const auto bind = [&] {
            REQUIRE(host.bind(document.graph().findNode("mesh"), owner.getLocalBounds(), document.revision()));
        };
        bind();
        const auto before = trimeshEditorMorphPosition(host);
        const uint64_t revision = document.revision();
        const uint64_t graphRevision = document.graph().getRevision();
        const auto model = document.graph().findNode("mesh")->model;
        int publications {};
        document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
        auto* editor = dynamic_cast<TrimeshExpandedEditorComponent*>(host.component());
        REQUIRE(editor != nullptr);
        const auto content = editor->getLocalBounds().toFloat().withTrimmedTop(34.f).reduced(10.f, 8.f);
        const auto regions = TrimeshWidget::expandedControlHitRegions(content, true, "gain", 3);
        const int axis = action == "close-secondary" ? 1 : 0;
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const auto& value) {
            return value.kind == TrimeshExpandedHitRegionKind::MorphControl
                    && value.parameterId == ids[axis];
        });
        REQUIRE(region != regions.end());
        const auto rail = region->bounds.reduced(8.f, 12.f);
        const auto source = rail.getCentre();
        auto* input = editor->getComponentAt(source.roundToInt());
        REQUIRE(input != nullptr);
        REQUIRE(input->getComponentID() == "trimesh.morph." + String(ids[axis]));
        const auto inputSource = input->getLocalPoint(editor, source);
        input->mouseDown(curvePanelMouseEvent(
                *input, inputSource, ModifierKeys::leftButtonModifier, inputSource, false));
        Point<float> destination;
        for (const float value : { 0.2f, 0.8f }) {
            destination = { rail.getX() + value * rail.getWidth(), source.y };
            input->mouseDrag(curvePanelMouseEvent(
                    *input, input->getLocalPoint(editor, destination),
                    ModifierKeys::leftButtonModifier, inputSource, true));
            REQUIRE(trimeshEditorMorphPosition(host)[(size_t) axis] == Catch::Approx(value).margin(0.001f));
        }
        const auto moved = trimeshEditorMorphPosition(host);
        if (action == "reset-document") {
            input->mouseUp(curvePanelMouseEvent(
                    *input, input->getLocalPoint(editor, destination), {}, inputSource, true));
        }
        host.close();
        REQUIRE_FALSE(host.hasEditor());
        if (action == "reset-document") {
            previews.resetDocumentPreviews();
        }
        bind();
        Image image(Image::RGB, host.component()->getWidth(), host.component()->getHeight(), true);
        Graphics graphics(image);
        host.component()->paint(graphics);
        const auto reopened = trimeshEditorMorphPosition(host);
        REQUIRE(reopened == (action == "close-primary" ? moved : before));
        REQUIRE(document.revision() == revision);
        REQUIRE(document.graph().getRevision() == graphRevision);
        REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
        REQUIRE(document.graph().findNode("mesh")->model == model);
        for (const char* id : ids) {
            REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                    .floatValue(id) == Catch::Approx(0.5f));
        }
        REQUIRE(publications == 0);
        REQUIRE_FALSE(dispatcher.hasTransientEdit());
        REQUIRE_FALSE(document.canUndo());
        REQUIRE_FALSE(document.isDirty());
    }
}

TEST_CASE("Both mesh editors publish one undoable gesture for each visible cube face",
        "[cycle-v2][editor][morph-face][regression][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curves;
    for (const auto kind : { NodeKind::Envelope, NodeKind::TrilinearMesh }) {
        const String nodeId = kind == NodeKind::Envelope ? "env" : "mesh";
        for (int held = 0; held < 3; ++held) {
            CAPTURE(nodeId, held);
            Component owner;
            owner.setSize(1100, 800);
            if (kind == NodeKind::TrilinearMesh) {
                owner.addToDesktop(ComponentPeer::windowIsTemporary);
                owner.setVisible(true);
            }
            NodeGraph graph = kind == NodeKind::Envelope
                    ? TestSupport::authoredEnvelopeGraph()
                    : TestSupport::authoredTrimeshGraph();
            const char* ids[] { "yellow", "red", "blue" };
            for (const char* id : ids) {
                REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, nodeId, id, id, "0.5").succeeded());
            }
            if (kind == NodeKind::TrilinearMesh) {
                addUnrelatedInteractionState(graph);
            }
            GraphDocument document(std::move(graph));
            GraphCommandDispatcher dispatcher(document);
            RecordingPresentation presentation;
            EnvelopeEditorResources resources;
            TrimeshWidget mesh;
            resources.activeTrimesh = &mesh;
            if (kind == NodeKind::Envelope) {
                resources.curveEditorWidget(*document.graph().findNode(nodeId))
                        ->syncFromNode(*document.graph().findNode(nodeId));
            }
            NodeEditorCommandService service(owner, document, dispatcher, presentation, resources);
            NodeEditorHost host(owner, service, presentation, resources);
            REQUIRE(host.bind(document.graph().findNode(nodeId), owner.getLocalBounds(), document.revision()));
            auto* editor = host.component();
            Rectangle<float> diagram;
            if (kind == NodeKind::Envelope) {
                diagram = rectangleProperty(dynamic_cast<EnvelopeEditorComponent*>(editor)->automationState(), "morphPlaneBounds");
            } else {
                const auto content = editor->getLocalBounds().toFloat().withTrimmedTop(34.f).reduced(10.f, 8.f);
                diagram = mesh.morphDiagramBounds(content);
            }
            const MorphSpaceLayout space(diagram, MorphDimensions(3));
            const auto axes = MorphSpaceLayout::cubeFaceAxes(held);
            const auto source = space.cubeFacePoint(held, 0.5f, 0.5f);
            const auto parameters = [&] {
                const NodeParameterMap values(*dispatcher.editingGraph().findNode(nodeId));
                return std::array<float, 3> { values.floatValue(ids[0]), values.floatValue(ids[1]), values.floatValue(ids[2]) };
            };
            const auto before = parameters();
            const uint64_t documentRevision = document.revision();
            const uint64_t graphRevision = document.graph().getRevision();
            const auto model = document.graph().findNode(nodeId)->model;
            const auto surfaceRebuilds = kind == NodeKind::TrilinearMesh
                    ? mesh.panelRenderStatsForAutomation().surfaceRebuildCount : 0;
            int publications {};
            document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
            Image image(Image::RGB, editor->getWidth(), editor->getHeight(), true);
            Graphics graphics(image);
            auto* input = kind == NodeKind::TrilinearMesh
                    ? editor->getComponentAt(source.roundToInt()) : editor;
            REQUIRE(input != nullptr);
            if (kind == NodeKind::TrilinearMesh) {
                input->grabKeyboardFocus();
                auto* focused = Component::getCurrentlyFocusedComponent();
                REQUIRE(focused != nullptr);
                REQUIRE(focused->getComponentID() == input->getComponentID());
            }
            const auto inputSource = input->getLocalPoint(editor, source);
            input->mouseDown(curvePanelMouseEvent(*input, inputSource, ModifierKeys::leftButtonModifier, inputSource, false));
            REQUIRE(parameters() == before);
            Point<float> destination;
            for (const float value : { 0.25f, 1.f }) {
                auto next = before;
                next[(size_t) axes[0]] = value;
                next[(size_t) axes[1]] = 1.f - value;
                destination = source + space.project(next[1], next[2], next[0])
                        - space.project(before[1], before[2], before[0]);
                InteractionComplexityDiagnostics::reset();
                input->mouseDrag(curvePanelMouseEvent(*input, input->getLocalPoint(editor, destination),
                        ModifierKeys::leftButtonModifier, inputSource, true));
                if (kind == NodeKind::TrilinearMesh) {
                    editor->paint(graphics);
                }
                REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
                REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
                REQUIRE(InteractionComplexityDiagnostics::counts().modelSerializations == 0);
                auto actual = parameters();
                if (kind == NodeKind::Envelope) {
                    const auto local = dynamic_cast<EnvelopeEditorComponent*>(editor)->automationState();
                    actual = { (float) local.getProperty("yellowMorph", {}),
                            (float) local.getProperty("redMorph", {}), (float) local.getProperty("blueMorph", {}) };
                } else {
                    actual = trimeshEditorMorphPosition(host);
                    REQUIRE(parameters() == before);
                    REQUIRE(document.revision() == documentRevision);
                    REQUIRE(document.graph().getRevision() == graphRevision);
                    REQUIRE(dispatcher.editingGraph().getRevision() == graphRevision);
                    REQUIRE(document.graph().findNode(nodeId)->model == model);
                    REQUIRE(publications == 0);
                    REQUIRE_FALSE(document.canUndo());
                    REQUIRE(mesh.panelRenderStatsForAutomation().surfaceRebuildCount == surfaceRebuilds);
                    REQUIRE(InteractionComplexityDiagnostics::counts().audioSamplesCopied == 0);
                    REQUIRE(InteractionComplexityDiagnostics::counts().validationNodeVisits == 0);
                    REQUIRE(InteractionComplexityDiagnostics::counts().validationEdgeVisits == 0);
                }
                for (int axis = 0; axis < 3; ++axis) {
                    REQUIRE(actual[(size_t) axis] == Catch::Approx(next[(size_t) axis]).margin(1.e-5f));
                }
            }
            input->mouseUp(curvePanelMouseEvent(*input, input->getLocalPoint(editor, destination), {}, inputSource, true));
            const auto committed = parameters();
            REQUIRE(committed[(size_t) held] == before[(size_t) held]);
            REQUIRE(committed[(size_t) axes[0]] == Catch::Approx(1.f));
            REQUIRE(committed[(size_t) axes[1]] == Catch::Approx(0.f));
            REQUIRE(publications == 1);
            REQUIRE(document.revision() == documentRevision + 1);
            if (kind == NodeKind::TrilinearMesh) {
                REQUIRE(document.graph().findNode(nodeId)->model == model);
                REQUIRE(trimeshEditorMorphPosition(host) == committed);
            }
            REQUIRE(document.undo());
            REQUIRE(parameters() == before);
            REQUIRE_FALSE(document.canUndo());
        }
    }
}
