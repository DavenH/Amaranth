#include <catch2/catch_test_macros.hpp>

#include "../src/UI/NodeEditorHost.h"

using namespace CycleV2;
using namespace juce;

namespace {

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

class NullResources final : public NodeEditorResources {
public:
    Effect2DWidget* effect2DWidget(const Node&) override { return nullptr; }
    TrimeshWidget* trimeshWidget(const Node&) override { return nullptr; }
    TrimeshRenderProfile trimeshRenderProfile(const Node&) const override {
        return TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    }
    std::array<String, 6> trimeshGuideLabels(const Node&) override { return {}; }
};

Node node(String id, NodeKind kind) {
    Node result;
    result.id = std::move(id);
    result.kind = kind;
    return result;
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
