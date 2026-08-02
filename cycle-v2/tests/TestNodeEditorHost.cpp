#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Effect2D/CurveNodeEditors.h"
#include "../src/Nodes/Effect2D/CurveEditorPrimitives.h"
#include "../src/Nodes/Effect2D/CurveExpandedEditorComponent.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Effects/EffectPreviewRenderer.h"
#include "../src/Nodes/Unison/UnisonNode.h"
#include "../src/UI/NodeCanvasAutomationController.h"
#include "../src/UI/NodeCanvasAutomationInspector.h"
#include "../src/UI/NodeEditorHost.h"
#include "../src/UI/NodeParameterValue.h"
#include "../src/UI/NodePreviewResources.h"

#include <Curve/Curve.h>

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
    void refreshNodeEditorPresentation() override {}
    Point<float> nodeEditorCreationPosition() const override { return {}; }
    void rebindNodeEditor() override { ++rebinds; }
    void recordNodeEditorMovement(const String&, const String&, uint64_t) override {
        ++recordedMovements;
    }

    int repaints {};
    int scheduledRefreshes {};
    int rebinds {};
    int recordedMovements {};
};

class NullResources final : public NodeEditorResources {
public:
    Effect2DWidget* effect2DWidget(const Node&) override { return nullptr; }
    TrimeshWidget* trimeshWidget(const Node&) override { return nullptr; }
    TrimeshRenderProfile trimeshRenderProfile(const Node&) const override {
        return TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    }
    std::array<String, 6> trimeshGuideLabels(const Node&) override { return {}; }
    void paintNodePreview(Graphics&, const Node&, Rectangle<float>) override {}
};

class RecordingCurveDelegate final : public CurveExpandedEditorDelegate {
public:
    void closeEffect2DEditor() override {}
    void repaintEffect2DEditorOpenGL() override { events.add("repaint"); }

    bool publishEffect2DState(
            NodeModelStatePtr,
            const std::vector<NodeParameter>&) override {
        events.add("publish");
        return true;
    }

    void beginEffect2DTransaction() override { events.add("begin"); }
    void commitEffect2DTransaction() override { events.add("commit"); }

    StringArray events;
};

class LifecycleCurveEditor final : public CurveExpandedEditorComponent {
public:
    explicit LifecycleCurveEditor(Effect2DWidget& widget) :
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
    const NodeCanvasAutomationPresentation state {
            "mesh",
            "mesh",
            "Opened editor: mesh",
            -1
    };
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

    auto hasTarget = [targets](const String& id) {
        return std::any_of(targets->begin(), targets->end(), [&](const var& targetValue) {
            const auto* target = targetValue.getDynamicObject();
            return target != nullptr && target->getProperty("id").toString() == id;
        });
    };

    REQUIRE(hasTarget("node:mesh"));
    REQUIRE(hasTarget("expanded:mesh.panel3D"));
    REQUIRE(hasTarget("expanded:mesh.trimeshMorphRail.yellow"));
    REQUIRE(hasTarget("expanded:mesh.trimeshVertexParameter.vertex.phase"));

    const var snapshot = inspector.exportState(state);
    const auto* snapshotObject = snapshot.getDynamicObject();
    REQUIRE(snapshotObject != nullptr);
    REQUIRE((int) snapshotObject->getProperty("nodeCount") == 1);
    REQUIRE(snapshotObject->getProperty("selectedNodeId").toString() == "mesh");
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
    Effect2DWidget widget(NodeKind::Waveshaper);
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
    const Node* baroqueGuide = baroque.findNode("guide1");
    const Node* stengahGuide = stengah.findNode("guide1");
    REQUIRE(baroqueGuide != nullptr);
    REQUIRE(stengahGuide != nullptr);

    Effect2DWidget widget(NodeKind::GuideCurve);
    auto editor = createCurveNodeEditor(NodeKind::GuideCurve, widget);
    editor->setBounds(0, 0, 640, 400);
    editor->setNode(*baroqueGuide);
    REQUIRE(widget.vertexCountForAutomation() == 4);
    REQUIRE(static_cast<double>(editor->automationState().getProperty("noise", {}))
            == Catch::Approx(0.76562));
    editor->setNode(*stengahGuide);
    REQUIRE(widget.vertexCountForAutomation() == 55);
    REQUIRE(static_cast<double>(editor->automationState().getProperty("noise", {}))
            == Catch::Approx(0.0025));

    widget.syncFromNode(*baroqueGuide);
    REQUIRE(static_cast<double>(widget.automationState().getProperty("firstControl", {}))
            == Catch::Approx(0.76562));
    Node revisedGuide = *stengahGuide;
    for (auto& parameter : revisedGuide.parameters) {
        if (parameter.id == "dcOffset") {
            parameter.value = "0.25";
        } else if (parameter.id == "phase") {
            parameter.value = "0.75";
        }
    }
    widget.syncFromNode(revisedGuide);
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
    REQUIRE(commands.publishCurveState(
            "shape",
            CurveNodeModelState::copyOf(model, model.revision()),
            curveControls(*document.graph().findNode("shape"))));
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
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
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

    REQUIRE(nodeParameterValue(*document.graph().findNode("reverb"), "wet") == "0.700000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(nodeParameterValue(*document.graph().findNode("reverb"), "wet") == "0.4");
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
    const String originalWidth = nodeParameterValue(
            *document.graph().findNode("unison"), "width");
    const uint64_t originalRevision = document.revision();
    std::vector<float> observedDetunes;
    const auto observePreview = [&] {
        const auto paths = makeUnisonPreviewPaths(
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

    REQUIRE(nodeParameterValue(*document.graph().findNode("unison"), "width") == "60.000000");
    REQUIRE(presentation.recordedMovements == 3);
    REQUIRE(presentation.repaints == 3);
    REQUIRE(presentation.rebinds == 1);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE(nodeParameterValue(*document.graph().findNode("unison"), "width") == originalWidth);
    REQUIRE_FALSE(document.canUndo());
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
    const String originalGain = nodeParameterValue(
            *document.graph().findNode("equalizer"), "band3Gain");
    const String originalFrequency = nodeParameterValue(
            *document.graph().findNode("equalizer"), "band3Frequency");

    REQUIRE(commands.beginNodeParameterPairEdit(
            "equalizer", "band3Gain", "Band 3 Gain", 0.6f,
            "band3Frequency", "Band 3 Frequency", 0.4f));
    REQUIRE(commands.updateNodeParameterPairEditValues(0.75f, 0.65f));
    commands.endNodeParameterEdit();

    const Node* edited = document.graph().findNode("equalizer");
    REQUIRE(nodeParameterValue(*edited, "band3Gain") == "0.750000");
    REQUIRE(nodeParameterValue(*edited, "band3Frequency") == "0.650000");
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    const Node* restored = document.graph().findNode("equalizer");
    REQUIRE(nodeParameterValue(*restored, "band3Gain") == originalGain);
    REQUIRE(nodeParameterValue(*restored, "band3Frequency") == originalFrequency);
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
    REQUIRE(nodeParameterValue(*document.graph().findNode("delay"), "time") == "0.250000");
    REQUIRE(presentation.rebinds == 2);

    REQUIRE(document.undo());
    REQUIRE(nodeParameterValue(*document.graph().findNode("delay"), "time") == "0.5");
    REQUIRE(nodeParameterValue(*document.graph().findNode("delay"), "enabled") == "0");

    REQUIRE(document.undo());
    REQUIRE(nodeParameterValue(*document.graph().findNode("delay"), "enabled") == "1");
}
