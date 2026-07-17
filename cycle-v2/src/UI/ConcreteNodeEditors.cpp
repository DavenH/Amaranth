#include "NodeEditorHost.h"

#include "NodeParameterValue.h"
#include "../Nodes/Effect2D/CurveNodeEditors.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshExpandedEditorComponent.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"

namespace CycleV2 {

namespace {

class PreviewNodeEditorComponent final : public Component {
public:
    PreviewNodeEditorComponent(
            NodeEditorPresentation& presentationToUse,
            NodeEditorResources& resourcesToUse) :
            presentation(presentationToUse)
        ,   resources(resourcesToUse) {
    }

    void setNode(const Node& nodeToUse) {
        node = nodeToUse;
        repaint();
    }

    void paint(Graphics& graphics) override {
        const Rectangle<float> panel = getLocalBounds().toFloat();
        graphics.setColour(Colour(0xff141a21));
        graphics.fillRoundedRectangle(panel, 8.f);

        auto header = panel;
        header.setHeight(34.f);
        graphics.setColour(Colour(0xff202833));
        graphics.fillRoundedRectangle(header, 8.f);
        graphics.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));
        graphics.setColour(Colour(0xffe2e8ef));
        graphics.setFont(FontOptions(14.f, Font::bold));
        graphics.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);

        const Rectangle<float> close = closeButtonBounds(panel);
        graphics.setColour(Colour(0xff0e1318));
        graphics.fillEllipse(close);
        graphics.setColour(Colour(0xffe2e8ef));
        graphics.drawEllipse(close, 1.f);
        graphics.drawLine(close.getX() + 7.f, close.getY() + 7.f,
                close.getRight() - 7.f, close.getBottom() - 7.f, 1.4f);
        graphics.drawLine(close.getRight() - 7.f, close.getY() + 7.f,
                close.getX() + 7.f, close.getBottom() - 7.f, 1.4f);

        resources.paintNodePreview(
                graphics,
                node,
                panel.withTrimmedTop(header.getHeight()).reduced(18.f, 16.f));
        graphics.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
        graphics.drawRoundedRectangle(panel.reduced(0.75f), 8.f, 1.3f);
    }

    void mouseDown(const MouseEvent& event) override {
        if (closeButtonBounds(getLocalBounds().toFloat()).contains(event.position)) {
            presentation.closeNodeEditor();
        }
    }

private:
    static Rectangle<float> closeButtonBounds(Rectangle<float> panel) {
        return Rectangle<float>(22.f, 22.f).withCentre({
                panel.getRight() - 22.f,
                panel.getY() + 17.f
        });
    }

    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    Node node;
};

class PreviewNodeEditor final : public NodeEditor {
public:
    explicit PreviewNodeEditor(const NodeEditorContext& context) :
            editor(context.presentation, context.resources) {
    }

    Component& component() override { return editor; }
    void bind(const Node& node) override { editor.setNode(node); }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject&) const override {}
    Rectangle<float> panelBoundsForAutomation() const override { return {}; }
    void releaseOpenGLResources() override {}

private:
    PreviewNodeEditorComponent editor;
};

class PreviewNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<PreviewNodeEditor>(context);
    }
};

class CurveNodeEditor final : public NodeEditor,
                              private CurveExpandedEditorDelegate {
public:
    CurveNodeEditor(
            const Node& node,
            const NodeEditorContext& context) :
            commands     (context.commands)
        ,   presentation (context.presentation)
        ,   editor       (createCurveNodeEditor(node.kind, *context.resources.effect2DWidget(node))) {
        editor->setDelegate(this);
    }

    Component& component() override { return *editor; }

    void bind(const Node& node) override {
        nodeId = node.id;
        editor->setNode(node);
    }

    void renderOpenGL(float scaleFactor) override { editor->renderOpenGL(scaleFactor); }
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("effect2D", editor->automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor->panelBoundsForAutomation();
    }
    void releaseOpenGLResources() override {}

private:
    void closeEffect2DEditor() override {
        presentation.closeNodeEditor();
    }

    void repaintEffect2DEditorOpenGL() override {
        presentation.repaintNodeEditor(true);
    }

    bool publishEffect2DState(
            const String& snapshot,
            uint64_t revision,
            const std::vector<NodeParameter>& controls) override {
        return commands.publishCurveState(nodeId, snapshot, revision, controls);
    }

    void beginEffect2DTransaction() override {
        commands.beginCurveTransaction();
    }

    void commitEffect2DTransaction() override {
        commands.commitCurveTransaction();
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    std::unique_ptr<CurveExpandedEditorComponent> editor;
    String nodeId;
};

class CurveNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        return std::make_unique<CurveNodeEditor>(node, context);
    }
};

class TrimeshNodeEditor final : public NodeEditor,
                                private TrimeshExpandedEditorDelegate {
public:
    TrimeshNodeEditor(
            const Node& node,
            const NodeEditorContext& context) :
            commands     (context.commands)
        ,   presentation (context.presentation)
        ,   resources    (context.resources)
        ,   editor       (std::make_unique<TrimeshExpandedEditorComponent>(
                    *context.resources.trimeshWidget(node))) {
        editor->setDelegate(this);
    }

    Component& component() override { return *editor; }

    void bind(const Node& node) override {
        nodeId = node.id;
        auto* widget = resources.trimeshWidget(node);
        jassert(widget != nullptr);
        boundNode = node;
        boundWidget = widget;
        widget->setMeshEditedCallback([this] { commands.persistTrimeshMeshEdits(nodeId); });
        editor->setRenderProfile(resources.trimeshRenderProfile(node));
        editor->setGuideAttachmentLabels(resources.trimeshGuideLabels(node));
        editor->setNode(node);
    }

    void renderOpenGL(float scaleFactor) override { editor->renderOpenGL(scaleFactor); }
    void appendAutomationState(DynamicObject& state) const override {
        if (boundWidget == nullptr) {
            return;
        }
        Array<var> morphSliders;
        Array<var> primaryAxisButtons;
        Array<var> linkToggles;
        for (const auto& axis : { String("yellow"), String("red"), String("blue") }) {
            auto* slider = new DynamicObject();
            slider->setProperty("id", axis);
            slider->setProperty("value", nodeParameterValue(boundNode, axis, "0.5").getDoubleValue());
            slider->setProperty("minimum", 0.0);
            slider->setProperty("maximum", 1.0);
            morphSliders.add(slider);

            auto* primary = new DynamicObject();
            primary->setProperty("id", axis);
            primary->setProperty("selected", nodeParameterValue(boundNode, "primaryAxis", "yellow") == axis);
            primaryAxisButtons.add(primary);

            auto* link = new DynamicObject();
            const String defaultValue = axis == "yellow" ? "1" : "0";
            link->setProperty("id", axis);
            link->setProperty(
                    "selected",
                    nodeParameterValue(boundNode, "link." + axis, defaultValue).getIntValue() != 0);
            linkToggles.add(link);
        }
        state.setProperty("morphSliders", morphSliders);
        state.setProperty("primaryAxisButtons", primaryAxisButtons);
        state.setProperty("linkToggles", linkToggles);

        auto* meshState = new DynamicObject();
        const int vertexCount = static_cast<int>(boundWidget->vertexMarkers().size());
        const int selectedVertexIndex = boundWidget->selectedVertexIndexForPanel();
        meshState->setProperty("vertexCount", vertexCount);
        meshState->setProperty("selectedVertexIndex", selectedVertexIndex);
        Array<var> selectedParameters;
        for (const auto& parameter : boundWidget->vertexParametersForIndex(selectedVertexIndex)) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("id", parameter.id);
            encoded->setProperty("value", parameter.value);
            selectedParameters.add(encoded);
        }
        meshState->setProperty("selectedVertexParameters", selectedParameters);
        Array<var> vertexMarkers;
        for (const auto& marker : boundWidget->vertexMarkers()) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("index", marker.index);
            encoded->setProperty("phase", marker.phase);
            encoded->setProperty("amp", marker.amp);
            vertexMarkers.add(encoded);
        }
        meshState->setProperty("vertexMarkers", vertexMarkers);
        const auto& slice = boundWidget->renderDataForAutomation().slice;
        float sliceMinimum {};
        float sliceMaximum {};
        double sliceAbsoluteSum {};
        if (!slice.empty()) {
            sliceMinimum = slice.front();
            sliceMaximum = slice.front();
            for (float sample : slice) {
                sliceMinimum = jmin(sliceMinimum, sample);
                sliceMaximum = jmax(sliceMaximum, sample);
                sliceAbsoluteSum += sample < 0.f ? -sample : sample;
            }
        }
        meshState->setProperty("sliceSampleCount", (int) slice.size());
        meshState->setProperty("sliceMinimum", sliceMinimum);
        meshState->setProperty("sliceMaximum", sliceMaximum);
        meshState->setProperty("sliceAbsoluteSum", sliceAbsoluteSum);
        const auto panelStats = boundWidget->panelRenderStatsForAutomation();
        meshState->setProperty("panelSampleCount", panelStats.sampleCount);
        meshState->setProperty("panelInterceptCount", panelStats.interceptCount);
        meshState->setProperty("panelMinimum", panelStats.minimum);
        meshState->setProperty("panelMaximum", panelStats.maximum);
        meshState->setProperty("panelCentreSample", panelStats.centreSample);
        meshState->setProperty("panelAbsoluteSum", panelStats.absoluteSum);
        Array<var> panelIntercepts;
        for (const auto& intercept : panelStats.intercepts) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("x", intercept.x);
            encoded->setProperty("y", intercept.y);
            panelIntercepts.add(encoded);
        }
        meshState->setProperty("panelIntercepts", panelIntercepts);
        state.setProperty("trimesh", var(meshState));
    }
    Rectangle<float> panelBoundsForAutomation() const override { return {}; }
    void releaseOpenGLResources() override {}

private:
    void closeTrimeshEditor() override {
        presentation.closeNodeEditor();
    }

    void repaintTrimeshEditorOpenGL() override {
        presentation.repaintNodeEditor(true);
    }

    void setTrimeshPrimaryAxisValue(const String& axis) override {
        commands.setTrimeshPrimaryAxisValue(nodeId, axis);
    }

    void toggleTrimeshLinkAxisValue(const String& axis) override {
        commands.toggleTrimeshLinkAxisValue(nodeId, axis);
    }

    void beginTrimeshMorphEdit(const String& id, float value) override {
        commands.beginTrimeshMorphEdit(nodeId, id, value);
    }

    void updateTrimeshMorphEdit(float value) override {
        commands.updateTrimeshMorphEditValue(value);
    }

    void endTrimeshMorphEdit() override {
        commands.endTrimeshMorphEdit();
    }

    void beginTrimeshVertexParameterEdit(const String& id, float value) override {
        commands.beginTrimeshVertexParameterEdit(nodeId, id, value);
    }

    void updateTrimeshVertexParameterEdit(float value) override {
        commands.updateTrimeshVertexParameterEditValue(value);
    }

    void endTrimeshVertexParameterEdit() override {
        commands.endTrimeshVertexParameterEdit();
    }

    void showTrimeshGuideAttachmentMenu(
            const String& field,
            Rectangle<int> area) override {
        commands.showTrimeshGuideAttachmentMenu(nodeId, field, area);
    }

    void selectTrimeshVertex(int index) override {
        commands.selectTrimeshVertexIndex(nodeId, index);
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    std::unique_ptr<TrimeshExpandedEditorComponent> editor;
    String nodeId;
    Node boundNode;
    TrimeshWidget* boundWidget {};
};

class TrimeshNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        return std::make_unique<TrimeshNodeEditor>(node, context);
    }
};

}

const NodeEditorFactoryRegistry& NodeEditorFactoryRegistry::instance() {
    static const NodeEditorFactoryRegistry registry;
    return registry;
}

NodeEditorFactoryRegistry::NodeEditorFactoryRegistry() {
    factories.emplace_back(NodeKind::Spy, std::make_unique<PreviewNodeEditorFactory>());
    factories.emplace_back(NodeKind::Envelope, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::GuideCurve, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::ImpulseResponse, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::Waveshaper, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::TrilinearMesh, std::make_unique<TrimeshNodeEditorFactory>());
}

const NodeEditorFactory* NodeEditorFactoryRegistry::find(NodeKind kind) const {
    for (const auto& entry : factories) {
        if (entry.first == kind) {
            return entry.second.get();
        }
    }
    return nullptr;
}

}
