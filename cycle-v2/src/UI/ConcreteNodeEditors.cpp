#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Curve/Editor/CurveNodeEditorFactory.h"
#include "Nodes/Delay/DelayNodeEditor.h"
#include "Nodes/Equalizer/EqualizerNodeEditor.h"
#include "Nodes/Reverb/ReverbNodeEditor.h"
#include "Nodes/Trimesh/Editor/TrimeshExpandedEditorComponent.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "Nodes/Unison/UnisonNodeEditor.h"
#include "Nodes/VoiceContext/Editor/VoiceContextNodeEditor.h"
#include "Runtime/NodePreviewProcessor.h"
#include "UI/ModulationNodeEditors.h"
#include "UI/NodeEditorHost.h"

namespace CycleV2 {

namespace {

class CurveNodeEditor final : public NodeEditor,
                              private CurveExpandedEditorDelegate {
public:
    CurveNodeEditor(
            const Node& node,
            const NodeEditorContext& context) :
            commands     (context.commands)
        ,   presentation (context.presentation)
        ,   resources    (context.resources)
        ,   editor       (createCurveNodeEditor(node.kind, *context.resources.curveEditorWidget(node))) {
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
    void closeCurveEditor() override {
        presentation.closeNodeEditor();
    }

    void repaintCurveEditorOpenGL() override {
        presentation.repaintNodeEditor(true);
    }

    bool publishCurveState(
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) override {
        return commands.publishCurveState(nodeId, std::move(model), controls);
    }

    void beginCurveTransaction() override {
        commands.beginCurveTransaction();
    }

    void commitCurveTransaction() override {
        commands.commitCurveTransaction();
    }

    void curveTransientStateChanged(uint64_t fingerprint) override {
        presentation.recordNodeEditorMovement(nodeId, "curve", fingerprint);
    }

    void setCurveEditorStatus(const String& message) override {
        presentation.setNodeEditorStatus(message);
    }

    bool setAudioResource(NodeAudioResourceEdit edit) override {
        return commands.setNodeAudioResource(std::move(edit));
    }

    bool removeAudioResource() override {
        return commands.removeNodeAudioResource(nodeId);
    }

    std::optional<NodeAudioResourceSummary> audioResourceSummary() const override {
        return resources.audioResourceSummary(nodeId);
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
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
        widget->setMeshEditedCallback([this](TrimeshMeshEditEvent event) {
            commands.persistTrimeshMeshEdits(nodeId, event.gestureComplete);
        });
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
        const NodeParameterMap parameters(boundNode);
        for (const auto& axis : { String("yellow"), String("red"), String("blue") }) {
            auto* slider = new DynamicObject();
            slider->setProperty("id", axis);
            slider->setProperty("value", parameters.floatValue(axis, 0.5f));
            slider->setProperty("minimum", 0.0);
            slider->setProperty("maximum", 1.0);
            morphSliders.add(slider);

            auto* primary = new DynamicObject();
            primary->setProperty("id", axis);
            primary->setProperty(
                    "selected",
                    parameters.stringValue("primaryAxis", "yellow") == axis);
            primaryAxisButtons.add(primary);

            auto* link = new DynamicObject();
            const String defaultValue = axis == "yellow" ? "1" : "0";
            link->setProperty("id", axis);
            link->setProperty(
                    "selected",
                    parameters.boolValue("link." + axis, defaultValue.getIntValue() != 0));
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
        meshState->setProperty("panelGuideRailSegmentCount", panelStats.guideRailSegmentCount);
        meshState->setProperty(
                "panelComponentGuideSegmentCount",
                panelStats.componentGuideSegmentCount);
        meshState->setProperty("panelCurveGuideSegmentCount", panelStats.curveGuideSegmentCount);
        meshState->setProperty("panelMinimum", panelStats.minimum);
        meshState->setProperty("panelMaximum", panelStats.maximum);
        meshState->setProperty("panelCentreSample", panelStats.centreSample);
        meshState->setProperty("panelPhaseUnitsPerDisplayX", panelStats.phaseUnitsPerDisplayX);
        meshState->setProperty("panelAmpUnitsPerDisplayY", panelStats.ampUnitsPerDisplayY);
        meshState->setProperty("panelAbsoluteSum", panelStats.absoluteSum);
        Array<var> panelIntercepts;
        for (const auto& intercept : panelStats.intercepts) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("x", intercept.x);
            encoded->setProperty("y", intercept.y);
            panelIntercepts.add(encoded);
        }
        meshState->setProperty("panelIntercepts", panelIntercepts);
        Array<var> panelDisplayedIntercepts;
        for (const auto& intercept : panelStats.displayedIntercepts) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("x", intercept.x);
            encoded->setProperty("y", intercept.y);
            panelDisplayedIntercepts.add(encoded);
        }
        meshState->setProperty("panelDisplayedIntercepts", panelDisplayedIntercepts);
        Array<var> panelDisplayedCurvePoints;
        for (const auto& point : panelStats.displayedCurvePoints) {
            auto* encoded = new DynamicObject();
            encoded->setProperty("x", point.x);
            encoded->setProperty("y", point.y);
            panelDisplayedCurvePoints.add(encoded);
        }
        meshState->setProperty("panelDisplayedCurvePoints", panelDisplayedCurvePoints);
        meshState->setProperty("panelCurveHover", panelStats.curveHover);
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
    factories.emplace_back(
            NodeKind::VoiceContext,
            createVoiceContextNodeEditorFactory());
    factories.emplace_back(
            NodeKind::ModulationSource,
            createModulationNodeEditorFactory());
    factories.emplace_back(
            NodeKind::ModulationTriple,
            createModulationNodeEditorFactory());
    factories.emplace_back(NodeKind::Envelope, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::ImpulseResponse, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::Waveshaper, std::make_unique<CurveNodeEditorFactory>());
    factories.emplace_back(NodeKind::TrilinearMesh, std::make_unique<TrimeshNodeEditorFactory>());
    factories.emplace_back(NodeKind::Unison, createUnisonNodeEditorFactory());
    factories.emplace_back(NodeKind::Reverb, createReverbNodeEditorFactory());
    factories.emplace_back(NodeKind::Delay, createDelayNodeEditorFactory());
    factories.emplace_back(NodeKind::Equalizer, createEqualizerNodeEditorFactory());
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
