#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Effect2D/CurveEditorPrimitives.h"
#include "../src/Nodes/Effect2D/CurveExpandedEditorComponent.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/UI/NodeEditorHost.h"

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
    bool publishCurveState(const String&, const String&, uint64_t,
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
    void persistTrimeshMeshEdits(const String&) override {}
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
    void rebindNodeEditor() override {}

    int repaints {};
    int scheduledRefreshes {};
};

class NullResources final : public NodeEditorResources {
public:
    Effect2DWidget* effect2DWidget(const Node&) override { return nullptr; }
    TrimeshWidget* trimeshWidget(const Node&) override { return nullptr; }
    TrimeshRenderProfile trimeshRenderProfile(const Node&) const override {
        return TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    }
    std::array<String, 6> trimeshGuideLabels(const Node&) override { return {}; }
};

class RecordingCurveDelegate final : public CurveExpandedEditorDelegate {
public:
    void closeEffect2DEditor() override {}
    void repaintEffect2DEditorOpenGL() override { events.add("repaint"); }

    bool publishEffect2DState(
            const String&,
            uint64_t,
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

std::vector<NodeParameter> curveControls(const Node& node) {
    std::vector<NodeParameter> controls;
    for (const auto& parameter : node.parameters) {
        if (parameter.id != CurveNodeModelCodec::snapshotParameterId()
                && parameter.id != CurveNodeModelCodec::revisionParameterId()) {
            controls.push_back(parameter);
        }
    }

    return controls;
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

    first.title = "Revised";
    REQUIRE(host.bind(&first, { 20, 30, 320, 220 }, 5));
    REQUIRE(stats.creations == 1);
    REQUIRE(stats.bindings == 2);
    REQUIRE(stats.boundNodeId == "curve-a");

    Node second = node("curve-b", NodeKind::Waveshaper);
    REQUIRE(host.bind(&second, { 0, 0, 120, 90 }, 5));
    REQUIRE(stats.creations == 2);
    REQUIRE(stats.destructions == 1);
    REQUIRE(host.isEditing("curve-b"));

    host.bind(nullptr, {});
    REQUIRE_FALSE(host.hasEditor());
    REQUIRE(stats.destructions == 2);
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
    REQUIRE(delegate.events == StringArray { "begin", "publish", "repaint", "commit" });

    delegate.events.clear();
    editor.toggle.button.onClick();
    REQUIRE(delegate.events == StringArray { "begin", "publish", "commit", "repaint" });

    delegate.events.clear();
    editor.menu.addItem("Four", 4);
    editor.menu.setSelectedId(4, sendNotificationSync);
    REQUIRE(delegate.events == StringArray { "begin", "publish", "commit", "repaint" });

    delegate.events.clear();
    editor.action.onClick();
    REQUIRE(editor.actionPerformed);
    REQUIRE(delegate.events == StringArray { "begin", "publish", "commit", "repaint" });
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
    model.setPublicationRevision(
            CurveNodeModelCodec::revisionFromParameters(
                    document.graph().findNode("shape")->parameters) + 1);

    commands.beginCurveTransaction();
    REQUIRE(commands.publishCurveState(
            "shape",
            model.snapshot(),
            model.revision(),
            curveControls(*document.graph().findNode("shape"))));
    REQUIRE(presentation.scheduledRefreshes == 0);
    REQUIRE(presentation.repaints == 1);
    commands.commitCurveTransaction();

    REQUIRE(presentation.scheduledRefreshes == 1);
    REQUIRE(presentation.repaints == 2);
    REQUIRE(document.canUndo());
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.canUndo());
}
