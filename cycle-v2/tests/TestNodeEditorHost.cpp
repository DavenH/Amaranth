#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Nodes/Curve/Editor/CurveNodeEditorFactory.h"
#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Unison/UnisonPreviewPainter.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "Nodes/Unison/UnisonNode.h"
#include "UI/NodeCanvasAutomationController.h"
#include "UI/NodeCanvasAutomationInspector.h"
#include "UI/EnvelopePurposeSelector.h"
#include "UI/NodeEditorHost.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodePreviewResources.h"

#include <Curve/Curve.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Mesh/Vertex.h>

using namespace CycleV2;
using namespace juce;

namespace {

class CurveTableScope {
public:
    CurveTableScope() { Curve::calcTable(); }
    ~CurveTableScope() { Curve::deleteTable(); }
};

struct EditorStats {
    int creations {};
    int destructions {};
    int bindings {};
    String boundNodeId;
};

class MockEditor final : public NodeEditor {
public:
    explicit MockEditor(EditorStats& statsToUse) : stats(statsToUse) { ++stats.creations; }
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

class NullCommands final : public NodeEditorCommands {
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

class NullPresentation final : public NodeEditorPresentation {
public:
    void closeNodeEditor() override {}
    void repaintNodeEditor(bool) override {}
    void selectEditedNode(const String&) override {}
    void setNodeEditorStatus(const String&) override {}
    void scheduleNodeEditorRefresh() override {}
    void flushNodeEditorRefresh() override {}
    void refreshNodeEditorPresentation() override {}
    Point<float> nodeEditorCreationPosition() const override { return {}; }
    void rebindNodeEditor() override {}
};

class RecordingPresentation final : public NodeEditorPresentation {
public:
    void closeNodeEditor() override {}
    void repaintNodeEditor(bool) override { ++repaints; }
    void selectEditedNode(const String&) override {}
    void setNodeEditorStatus(const String&) override {}
    void scheduleNodeEditorRefresh() override { ++scheduledRefreshes; }
    void flushNodeEditorRefresh() override {}
    void refreshNodeEditorPresentation() override { ++immediateRefreshes; }
    Point<float> nodeEditorCreationPosition() const override { return {}; }
    void rebindNodeEditor() override { ++rebinds; }
    void recordNodeEditorMovement(const String&, const String&, uint64_t) override {
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
    int recordedMovements {};
};

class NullResources final : public NodeEditorResources {
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
        return TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    }
    std::array<String, 6> trimeshGuideLabels(const Node&) override { return {}; }
    void paintNodePreview(Graphics&, const Node&, Rectangle<float>) override {}

    TrimeshWidget* activeTrimesh {};
    int synchronizingTrimeshLookups {};
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
        resources.setGraph(&document.graph());
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

class RecordingCurveDelegate final : public CurveExpandedEditorDelegate {
public:
    void closeCurveEditor() override {}
    void repaintCurveEditorOpenGL() override { events.add("repaint"); }

    bool publishCurveState(
            NodeModelStatePtr,
            const std::vector<NodeParameter>&) override {
        events.add("publish");
        return true;
    }

    void beginCurveTransaction() override { events.add("begin"); }
    void commitCurveTransaction() override { events.add("commit"); }

    StringArray events;
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

    const var diagnostics = automation.inspectOpenGLDiagnostics({ true, {} });
    const auto* object = diagnostics.getDynamicObject();
    REQUIRE(object != nullptr);
    REQUIRE(object->getProperty("schema").toString() == "cycle-v2-opengl-diagnostics.v1");
    REQUIRE((bool) object->getProperty("canvasOpenGlAttached"));
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
    NullCommands commands;
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
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
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
    state.guideDock.dockBounds = { 0.f, 610.f, 1200.f, 190.f };
    state.guideDock.guideShelfBounds = { 0.f, 610.f, 600.f, 190.f };
    state.guideDock.spyShelfBounds = { 600.f, 610.f, 600.f, 190.f };
    state.guideDock.dividerBounds = { 596.f, 610.f, 8.f, 190.f };
    state.guideDock.collapseBounds = { 1072.f, 618.f, 116.f, 22.f };
    state.guideDock.resizeBounds = { 0.f, 610.f, 1200.f, 7.f };
    state.guideDock.guideMinimizeBounds = { 12.f, 620.f, 18.f, 18.f };
    state.guideDock.spyMinimizeBounds = { 612.f, 620.f, 18.f, 18.f };
    state.guideDock.addGuideBounds = { 566.f, 618.f, 22.f, 22.f };
    state.guideDock.expandedGuideId = "guide1";
    state.guideDock.guideEditorBounds = { 36.f, 24.f, 1128.f, 562.f };
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
    REQUIRE(targetWithId("guideDock") != nullptr);
    REQUIRE(targetWithId("guideDock.collapse") != nullptr);
    REQUIRE(targetWithId("guideDock.resize") != nullptr);
    REQUIRE(targetWithId("guideDock.divider") != nullptr);
    REQUIRE(targetWithId("guideShelf.minimize") != nullptr);
    REQUIRE(targetWithId("spyShelf.minimize") != nullptr);
    REQUIRE(targetWithId("guideShelf.add") != nullptr);
    REQUIRE(targetWithId("guide:guide1") != nullptr);
    REQUIRE(targetWithId("guideEditor:guide1") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.panel3D") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshMorphRail.yellow") != nullptr);
    REQUIRE(targetWithId("expanded:mesh.trimeshVertexParameter.vertex.phase") != nullptr);
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
    REQUIRE(widget.vertexCountForAutomation() == 55);
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
    guide.model = createDefaultGuideCurveModel();

    const Rectangle<float> host = GuideCurveEditorComponent::preferredHostBounds(
            { 0.f, 0.f, 1200.f, 800.f });
    REQUIRE(host.getHeight() == 560.f);
    REQUIRE(host.getCentreY() == 400.f);

    editor.setBounds(host.toNearestInt());
    editor.setGuideResource(guide);

    int sliderCount = 0;
    int textButtonCount = 0;
    int emptyToggleCount = 0;
    int enabledLabelCount = 0;
    int lowestControlBottom = 0;
    for (int index = 0; index < editor.getNumChildComponents(); ++index) {
        Component* child = editor.getChildComponent(index);
        if (auto* slider = dynamic_cast<Slider*>(child)) {
            ++sliderCount;
            REQUIRE(slider->getTextBoxPosition() == Slider::TextBoxRight);
            lowestControlBottom = jmax(lowestControlBottom, slider->getBottom());
        } else if (dynamic_cast<TextButton*>(child) != nullptr) {
            ++textButtonCount;
        } else if (auto* toggle = dynamic_cast<ToggleButton*>(child)) {
            emptyToggleCount += toggle->getButtonText().isEmpty() ? 1 : 0;
            lowestControlBottom = jmax(lowestControlBottom, toggle->getBottom());
        } else if (auto* label = dynamic_cast<Label*>(child)) {
            enabledLabelCount += label->getText() == "Enabled" ? 1 : 0;
        }
    }
    REQUIRE(sliderCount == 3);
    REQUIRE(textButtonCount == 0);
    REQUIRE(emptyToggleCount == 1);
    REQUIRE(enabledLabelCount == 1);
    REQUIRE(lowestControlBottom < 230);
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
    REQUIRE(graphEditor.setNodeParameter(
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
            CurveNodeModelState::copyOf(envelopeModel, envelopeModel.revision() + 1)));

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
    editor->setBounds(0, 0, 640, 400);
    editor->setNode(*graph.findNode("env"));
    const var state = editor->automationState();
    panelState = widget.automationState();
    REQUIRE((bool) panelState.getProperty("hasDisplayCoordinates", {}));
    REQUIRE(state.getProperty("purpose", {}).toString() == "Pitch");
    REQUIRE(state.getProperty("polarity", {}).toString() == "bipolar");
    const auto purposeBounds = rectangleProperty(state, "purposeBounds");
    const auto blueMorphBounds = rectangleProperty(state, "blueMorphBounds");
    const auto actionRowBounds = rectangleProperty(state, "actionRowBounds");
    REQUIRE(purposeBounds.getWidth() > 0.f);
    REQUIRE(blueMorphBounds.getWidth() > 0.f);
    REQUIRE(actionRowBounds.getWidth() > 0.f);
    REQUIRE(purposeBounds.getBottom() < blueMorphBounds.getY());
    REQUIRE(blueMorphBounds.getBottom() < actionRowBounds.getY());
    panelState = widget.automationState();
    REQUIRE((bool) panelState.getProperty("bipolar", {}));
    REQUIRE(static_cast<double>(panelState.getProperty("verticalZoomHeight", {})) < 0.1);
    EnvelopePurposeSelector* modeSelector = nullptr;
    Button* pitchMode = nullptr;
    Button* scratchMode = nullptr;
    ImageButton* loopMarker = nullptr;
    ImageButton* sustainMarker = nullptr;
    ImageButton* fitVertical = nullptr;
    ImageButton* fullVertical = nullptr;
    StringArray actionLabels;
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
        } else if (auto* button = dynamic_cast<TextButton*>(editor->getChildComponent(index))) {
            actionLabels.add(button->getButtonText());
        } else if (auto* button = dynamic_cast<ImageButton*>(editor->getChildComponent(index))) {
            if (button->getName() == "Set selected vertex as loop start") {
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
    REQUIRE(modeSelector->getNumChildComponents() == 4);
    REQUIRE(pitchMode != nullptr);
    REQUIRE(scratchMode != nullptr);
    REQUIRE(loopMarker != nullptr);
    REQUIRE(sustainMarker != nullptr);
    REQUIRE(fitVertical != nullptr);
    REQUIRE(fullVertical != nullptr);
    REQUIRE(actionLabels == StringArray({ "Log" }));
    REQUIRE(loopMarker->getNormalImage().isValid());
    REQUIRE(sustainMarker->getNormalImage().isValid());
    REQUIRE_FALSE(loopMarker->isEnabled());
    REQUIRE_FALSE(sustainMarker->isEnabled());
    REQUIRE(loopMarker->getTooltip().containsIgnoreCase("select one envelope vertex"));
    REQUIRE(sustainMarker->getTooltip().containsIgnoreCase("select one envelope vertex"));
    const auto fitBounds = rectangleProperty(state, "fitVerticalBounds");
    const auto fullBounds = rectangleProperty(state, "fullVerticalBounds");
    const auto modeBounds = rectangleProperty(state, "modeBounds");
    REQUIRE(state.getProperty("modeLabel", {}).toString() == "Mode");
    REQUIRE(state.getProperty("vertexModeLabel", {}).toString() == "Vertex");
    REQUIRE_FALSE((bool) state.getProperty("loopEnabled", {}));
    REQUIRE_FALSE((bool) state.getProperty("sustainEnabled", {}));
    REQUIRE(modeBounds == purposeBounds);
    const var modeOptions = state.getProperty("modeOptions", {});
    REQUIRE(modeOptions.isArray());
    REQUIRE(modeOptions.getArray()->size() == 4);
    REQUIRE(fitBounds.getWidth() == Catch::Approx(28.f));
    REQUIRE(fullBounds.getWidth() == Catch::Approx(28.f));
    REQUIRE(fitBounds.getY() >= actionRowBounds.getY());
    REQUIRE(fullBounds.getBottom() <= actionRowBounds.getBottom());
    REQUIRE(fitBounds.getY() > purposeBounds.getBottom());
    const auto markerGroupBounds = rectangleProperty(state, "vertexModeGroupBounds");
    const auto logarithmicBounds = rectangleProperty(state, "logarithmicBounds");
    const auto rangeGroupBounds = rectangleProperty(state, "rangeGroupBounds");
    REQUIRE(markerGroupBounds.getRight() < logarithmicBounds.getX());
    REQUIRE(logarithmicBounds.getRight() < rangeGroupBounds.getX());
    const auto parameterRails = state.getProperty("vertexParameterRails", {});
    REQUIRE(parameterRails.isArray());
    REQUIRE(parameterRails.getArray()->size() >= 2);
    const auto vertexParameterBounds = rectangleProperty(state, "vertexParameterBounds");
    REQUIRE(vertexParameterBounds.getHeight() == Catch::Approx(230.f));
    const auto firstRail = rectangleProperty(parameterRails.getArray()->getReference(0), "bounds");
    const auto secondRail = rectangleProperty(parameterRails.getArray()->getReference(1), "bounds");
    REQUIRE(secondRail.getY() - firstRail.getY() == Catch::Approx(33.54f).margin(0.02f));
    REQUIRE((bool) panelState.getProperty("previewPreservesInteractiveZoom", {}));

    VertCube* selectedCube = envelopeModel.getMesh().getCubes().front();
    REQUIRE(selectedCube != nullptr);
    REQUIRE(envelopeModel.synchronizeFromMesh(selectedCube));
    REQUIRE(envelopeModel.selectedCubeId().has_value());
    Node selectedNode = *graph.findNode("env");
    selectedNode.model = CurveNodeModelState::copyOf(
            envelopeModel, selectedNode.model->revision() + 1);
    auto* selectedEditorState = new DynamicObject();
    selectedEditorState->setProperty(
            "selectedCubeId", (int64) *envelopeModel.selectedCubeId());
    selectedNode.editorState = var(selectedEditorState);
    editor->setNode(selectedNode);
    const var selectedState = editor->automationState();
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
}

TEST_CASE("Logarithmic Envelope grid distinguishes major divisions",
        "[cycle-v2][node-editor-host][envelope][logarithmic][grid]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTable;
    GraphNodeFactory factory;
    GraphEditor graphEditor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(graphEditor.setNodeParameter(
            graph, "env", "logarithmic", "Logarithmic", "1").succeeded());

    CurveEditorWidget widget(NodeKind::Envelope);
    auto editor = createCurveNodeEditor(NodeKind::Envelope, widget);
    editor->setBounds(0, 0, 640, 400);
    editor->setNode(*graph.findNode("env"));

    const var panelState = widget.automationState();
    REQUIRE((int) panelState.getProperty("horizontalMinorGridLineCount", {}) == 12);
    REQUIRE((int) panelState.getProperty("horizontalMajorGridLineCount", {}) == 4);
    REQUIRE(static_cast<double>(panelState.getProperty("minorGridBrightness", {}))
            == Catch::Approx(0.085 * 1.2));
    REQUIRE(static_cast<double>(panelState.getProperty("majorGridBrightness", {}))
            == Catch::Approx(0.14));
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

TEST_CASE("Node editor command service publishes model edits as one transaction",
        "[cycle-v2][editor][model]") {
    ScopedJuceInitialiser_GUI juce;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Unison, "unison", {}));
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

    commands.beginNodeModelEdit();
    REQUIRE(commands.publishNodeModel(
            "unison",
            UnisonNodeModelState::create({ {}, {} }, 2)));
    REQUIRE(commands.publishNodeModel(
            "unison",
            UnisonNodeModelState::create({ {}, {}, {} }, 3)));
    commands.endNodeModelEdit();

    const auto current = std::dynamic_pointer_cast<const UnisonNodeModelState>(
            document.graph().findNode("unison")->model);
    REQUIRE(current != nullptr);
    REQUIRE(current->voices().size() == 3);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    const auto restored = std::dynamic_pointer_cast<const UnisonNodeModelState>(
            document.graph().findNode("unison")->model);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->voices().size() == 1);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Trimesh primary morph commits refresh graph presentation",
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
    NullResources resources;
    NodeEditorCommandService commands(
            owner,
            document,
            dispatcher,
            presentation,
            resources);

    REQUIRE(commands.beginTrimeshMorphEdit("mesh", "yellow", 0.8f));
    commands.endTrimeshMorphEdit();

    REQUIRE(parameterValueForNode(*document.graph().findNode("mesh"), "yellow") == "0.800");
    REQUIRE(presentation.recordedMovements == 1);
    REQUIRE(presentation.immediateRefreshes == 0);
    REQUIRE(presentation.localCommits == 1);
}

TEST_CASE("Live Trimesh morph commits reuse movement refresh",
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

    REQUIRE(presentation.recordedMovements == 1);
    REQUIRE(presentation.immediateRefreshes == 0);
    REQUIRE(presentation.localCommits == 1);
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
    REQUIRE(presentation.scheduledRefreshes == 0);
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(presentation.rebinds == 1);
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
    REQUIRE(presentation.rebinds == 1);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(parameterValueForNode(*document.graph().findNode("unison"), "width") == originalWidth);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Trimesh drag publishes successive active-mesh snapshots without resynchronizing",
        "[cycle-v2][editor][trimesh][regression]") {
    ScopedJuceInitialiser_GUI juce;
    CurveTableScope curveTables;
    Component owner;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {}));
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
    const float originalAmp = durableModel->mesh().getVerts().front()->values[Vertex::Amp];

    widget.currentMesh().getVerts().front()->values[Vertex::Amp] = originalAmp + 0.05f;
    commands.persistTrimeshMeshEdits("mesh", false);
    const auto firstTransient = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            dispatcher.editingGraph().findNode("mesh")->model);
    REQUIRE(firstTransient != nullptr);
    REQUIRE(firstTransient->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp + 0.05f));
    REQUIRE(document.graph().findNode("mesh")->model->revision() == durableModel->revision());

    widget.currentMesh().getVerts().front()->values[Vertex::Amp] = originalAmp + 0.1f;
    commands.persistTrimeshMeshEdits("mesh", true);
    const auto committed = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(committed != nullptr);
    REQUIRE(committed->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp + 0.1f));
    REQUIRE(resources.synchronizingTrimeshLookups == 0);
    REQUIRE(presentation.recordedMovements == 2);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());

    const auto restored = std::dynamic_pointer_cast<const TrimeshNodeModelState>(
            document.graph().findNode("mesh")->model);
    REQUIRE(restored != nullptr);
    REQUIRE(restored->mesh().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(originalAmp));
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
