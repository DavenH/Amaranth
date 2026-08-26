#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>
#include <iterator>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Curve/Editor/CurveNodeEditorFactory.h"
#include "Nodes/Delay/DelayNodeEditor.h"
#include "Nodes/Equalizer/EqualizerPreviewPainter.h"
#include "Nodes/Reverb/ReverbNodeEditor.h"
#include "Nodes/Trimesh/Editor/TrimeshExpandedEditorComponent.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "Nodes/Unison/UnisonNodeEditor.h"
#include "Runtime/NodePreviewProcessor.h"
#include "UI/ModulationNodeEditors.h"
#include "UI/NodeEditorHost.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

namespace {

class EqualizerParameterSlider final : public Slider {
public:
    void setEqualizerGain(bool shouldUseEqualizerGain) {
        equalizerGain = shouldUseEqualizerGain;
    }

    void setEqualizerFrequency(bool shouldUseEqualizerFrequency) {
        equalizerFrequency = shouldUseEqualizerFrequency;
    }

    double snapValue(double attemptedValue, DragMode dragMode) override {
        ignoreUnused(dragMode);
        if (equalizerGain) {
            return CycleDsp::equalizerGainSnappedUnitValue(
                    (float) attemptedValue,
                    (float) getWidth());
        }
        return attemptedValue;
    }

    void paint(Graphics& graphics) override {
        Slider::paint(graphics);
        if (equalizerGain) {
            graphics.setColour(EffectPlotPalette::label.withAlpha(0.82f));
            graphics.setFont(FontOptions(8.f));
            paintStopTick(graphics, 0.5, "0");
            return;
        }
        if (equalizerFrequency) {
            static constexpr float landmarks[] {
                    60.f, 120.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f
            };
            static constexpr const char* labels[] {
                    "60", "120", "250", "500", "1k", "2k", "4k", "8k", "16k"
            };
            graphics.setColour(EffectPlotPalette::label.withAlpha(0.64f));
            graphics.setFont(FontOptions(7.f));
            for (size_t index = 0; index < std::size(landmarks); ++index) {
                paintStopTick(
                        graphics,
                        CycleDsp::equalizerFrequencyUnitValue(landmarks[index]),
                        labels[index]);
            }
            return;
        }
    }

private:
    void paintStopTick(Graphics& graphics, double value, const String& text) {
        const float x = getPositionOfValue(value);
        graphics.drawVerticalLine(roundToInt(x), 10.f, 18.f);
        graphics.drawText(
                text,
                roundToInt(x) - 8,
                0,
                16,
                9,
                Justification::centred);
    }

    bool equalizerGain {};
    bool equalizerFrequency {};
};

class EqualizerEditorComponent final : public Component {
public:
    EqualizerEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse) :
            commands     (commandsToUse)
        ,   presentation (presentationToUse) {
        closeButton.setButtonText(String::fromUTF8("\xc3\x97"));
        closeButton.onClick = [this] { presentation.closeNodeEditor(); };
        addAndMakeVisible(closeButton);
        enabledButton.setButtonText("Enabled");
        enabledButton.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id, "enabled", "Enabled", enabledButton.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(enabledButton);
        gainHeader.setText("GAIN", dontSendNotification);
        frequencyHeader.setText("FREQUENCY", dontSendNotification);
        gainHeader.setColour(Label::textColourId, Colour(0xffaab4c0));
        frequencyHeader.setColour(Label::textColourId, Colour(0xffaab4c0));
        addAndMakeVisible(gainHeader);
        addAndMakeVisible(frequencyHeader);
        createControls();
    }

    void setNode(const Node& nodeToUse) {
        node = nodeToUse;
        const NodeParameterMap parameters(node);
        enabledButton.setToggleState(
                parameters.boolValue("enabled", true),
                dontSendNotification);
        for (auto& control : controls) {
            control->slider.setValue(
                    parameters.stringValue(control->id, String(control->defaultValue))
                            .getDoubleValue(),
                    dontSendNotification);
            updateReadout(*control);
        }
        repaint();
    }

    void paint(Graphics& graphics) override {
        graphics.fillAll(Colour(0xff11151b));
        graphics.setColour(Colour(0xff2b3340));
        graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.f, 1.f);
        graphics.setColour(Colour(0xffeef2f6));
        graphics.setFont(FontOptions(18.f));
        graphics.drawText(title(), 18, 10, getWidth() - 80, 28, Justification::centredLeft);
        if (node.id.isNotEmpty()) {
            auto response = Rectangle<float>(18.f, 52.f, (float) getWidth() - 36.f, 150.f);
            graphics.setColour(EffectPlotPalette::forEnabledState(
                    EffectPlotPalette::insetBackground,
                    enabledButton.getToggleState()));
            graphics.fillRoundedRectangle(response, 6.f);
            EqualizerPreviewPainter().paint(
                    graphics,
                    response.reduced(12.f, 9.f),
                    node,
                    true);
        }
    }

    void resized() override {
        closeButton.setBounds(getWidth() - 42, 9, 28, 28);
        enabledButton.setBounds(getWidth() - 142, 12, 88, 24);
        int y = 242;
        gainHeader.setBounds(38, y - 18, (getWidth() - 76) / 2, 18);
        frequencyHeader.setBounds(
                getWidth() / 2 + 32,
                y - 18,
                (getWidth() - 76) / 2,
                18);
        for (size_t band = 0; band < 5; ++band) {
            layoutEqualizerRow(band, y);
            y += 62;
        }
    }

    void mouseDown(const MouseEvent& event) override {
        if (!equalizerGraphArea().contains(event.position)) {
            return;
        }

        constexpr float markerHitRadius = 36.f;
        float nearestDistance = markerHitRadius;
        for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
            const float distance = EqualizerPreviewPainter()
                    .bandControlPoint(equalizerGraphArea(), node, band)
                    .getDistanceFrom(event.position);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                draggedEqualizerBand = band;
            }
        }
        if (draggedEqualizerBand < 0) {
            return;
        }

        Control& gain = *controls[(size_t) draggedEqualizerBand * 2];
        Control& frequency = *controls[(size_t) draggedEqualizerBand * 2 + 1];
        commands.beginNodeParameterPairEdit(
                node.id,
                gain.id,
                gain.name,
                (float) gain.slider.getValue(),
                frequency.id,
                frequency.name,
                (float) frequency.slider.getValue());
    }

    void mouseDrag(const MouseEvent& event) override {
        if (draggedEqualizerBand < 0) {
            return;
        }

        const Rectangle<float> graph = equalizerGraphArea();
        const float frequencyPosition = jlimit(
                0.f,
                1.f,
                (event.position.x - graph.getX()) / graph.getWidth());
        const float gainValue = jlimit(
                0.f,
                1.f,
                (graph.getBottom() - event.position.y) / graph.getHeight());
        const float frequency = 40.f * std::pow(400.f, frequencyPosition);
        const float frequencyValue = CycleDsp::equalizerFrequencyUnitValue(frequency);
        Control& gain = *controls[(size_t) draggedEqualizerBand * 2];
        Control& frequencyControl = *controls[(size_t) draggedEqualizerBand * 2 + 1];
        gain.slider.setValue(gainValue, dontSendNotification);
        frequencyControl.slider.setValue(frequencyValue, dontSendNotification);
        updateReadout(gain);
        updateReadout(frequencyControl);
        setLocalNodeParameter(gain.id, gainValue);
        setLocalNodeParameter(frequencyControl.id, frequencyValue);
        commands.updateNodeParameterPairEditValues(gainValue, frequencyValue);
        repaint();
    }

    void mouseUp(const MouseEvent&) override {
        if (draggedEqualizerBand < 0) {
            return;
        }
        commands.endNodeParameterEdit();
        draggedEqualizerBand = -1;
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", title());
        state->setProperty("enabled", enabledButton.getToggleState());
        Array<var> values;
        for (const auto& control : controls) {
            auto* value = new DynamicObject();
            value->setProperty("id", control->id);
            value->setProperty("value", control->slider.getValue());
            value->setProperty("readout", control->readout.getText());
            values.add(value);
        }
        state->setProperty("controls", values);
        return state;
    }

private:
    struct Control {
        String id;
        String name;
        float defaultValue {};
        Label label;
        EqualizerParameterSlider slider;
        Label readout;
        bool editing {};
    };

    void addControl(const String& id, const String& name, float defaultValue) {
        auto control = std::make_unique<Control>();
        control->id = id;
        control->name = name;
        control->defaultValue = defaultValue;
        control->label.setText(name, dontSendNotification);
        control->label.setColour(Label::textColourId, Colour(0xffaab4c0));
        control->readout.setJustificationType(Justification::centredRight);
        control->readout.setColour(Label::textColourId, Colour(0xffeef2f6));
        control->slider.setSliderStyle(Slider::LinearHorizontal);
        control->slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        control->slider.setRange(0.0, 1.0, 0.0001);
        control->slider.setDoubleClickReturnValue(true, defaultValue);
        control->slider.setEqualizerGain(id.endsWith("Gain"));
        control->slider.setEqualizerFrequency(id.endsWith("Frequency"));
        if (id.endsWith("Frequency")) {
            String explanation = "Fixed-bandwidth centre frequency";
            if (id.startsWith("band1")) {
                explanation = "Low-shelf corner frequency";
            } else if (id.startsWith("band5")) {
                explanation = "High-shelf corner frequency";
            }
            control->label.setTooltip(explanation);
            control->slider.setTooltip(explanation);
        }
        auto* raw = control.get();
        control->slider.onDragStart = [this, raw] {
            raw->editing = true;
            commands.beginNodeParameterEdit(
                    node.id, raw->id, raw->name, (float) raw->slider.getValue());
        };
        control->slider.onValueChange = [this, raw] {
            updateReadout(*raw);
            const float value = (float) raw->slider.getValue();
            if (raw->editing) {
                commands.updateNodeParameterEditValue(value);
            } else {
                commands.setNodeParameterValue(node.id, raw->id, raw->name, value);
            }
            if (const auto* definition = NodeDefinitionRegistry::instance().findParameter(
                    NodeKind::Equalizer,
                    raw->id)) {
                const String normalized = definition->normalized(String(value, 6));
                for (auto& parameter : node.parameters) {
                    if (parameter.id == raw->id) {
                        parameter.value = normalized;
                        break;
                    }
                }
            }
            repaint();
        };
        control->slider.onDragEnd = [this, raw] {
            commands.endNodeParameterEdit();
            raw->editing = false;
        };
        addAndMakeVisible(control->label);
        addAndMakeVisible(control->slider);
        addAndMakeVisible(control->readout);
        controls.push_back(std::move(control));
    }

    void createControls() {
        const float frequencies[] { 60.f, 250.f, 1200.f, 4000.f, 8000.f };
        for (int band = 0; band < 5; ++band) {
            const String prefix = "band" + String(band + 1);
            addControl(prefix + "Gain", "Band " + String(band + 1) + " Gain", 0.5f);
            addControl(
                    prefix + "Frequency",
                    "Band " + String(band + 1) + " Frequency",
                    CycleDsp::equalizerFrequencyUnitValue(frequencies[band]));
            controls[controls.size() - 2]->label.setText(
                    String(band + 1),
                    dontSendNotification);
        }
    }

    void layoutEqualizerRow(size_t band, int y) {
        Control& gain = *controls[band * 2];
        Control& frequency = *controls[band * 2 + 1];
        const int columnWidth = (getWidth() - 76) / 2;
        const int frequencyX = getWidth() / 2 + 20;

        gain.label.setBounds(18, y + 20, 20, 28);
        gain.readout.setBounds(38 + columnWidth - 92, y, 92, 18);
        gain.slider.setBounds(38, y + 20, columnWidth, 28);
        frequency.label.setBounds(0, 0, 0, 0);
        frequency.readout.setBounds(frequencyX + columnWidth - 92, y, 92, 18);
        frequency.slider.setBounds(frequencyX, y + 20, columnWidth, 28);
    }

    Rectangle<float> equalizerGraphArea() const {
        return Rectangle<float>(18.f, 52.f, (float) getWidth() - 36.f, 150.f)
                .reduced(12.f, 9.f);
    }

    void setLocalNodeParameter(const String& id, float value) {
        for (NodeParameter& parameter : node.parameters) {
            if (parameter.id == id) {
                parameter.value = String(value, 6);
                return;
            }
        }
    }

    void updateReadout(Control& control) {
        const float value = (float) control.slider.getValue();
        String text;
        if (control.id.endsWith("Gain")) {
            const float gain = CycleDsp::equalizerGainDecibels(value);
            text = (gain > 0.f ? "+" : "") + String(gain, 1) + " dB";
        } else {
            const float frequency = CycleDsp::equalizerFrequency(value);
            text = frequency >= 1000.f
                    ? String(frequency / 1000.f, 2) + " kHz"
                    : String(roundToInt(frequency)) + " Hz";
        }
        control.readout.setText(text, dontSendNotification);
    }

    String title() const {
        return "EQUALIZER";
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    Node node;
    TextButton closeButton;
    ToggleButton enabledButton;
    Label gainHeader;
    Label frequencyHeader;
    int draggedEqualizerBand { -1 };
    std::vector<std::unique_ptr<Control>> controls;
};

class EqualizerNodeEditor final : public NodeEditor {
public:
    explicit EqualizerNodeEditor(const NodeEditorContext& context) :
            editor(context.commands, context.presentation) {}
    Component& component() override { return editor; }
    void bind(const Node& node) override { editor.setNode(node); }
    void renderOpenGL(float) override {}
    void appendAutomationState(DynamicObject& state) const override {
        state.setProperty("effectParameters", editor.automationState());
    }
    Rectangle<float> panelBoundsForAutomation() const override {
        return editor.getLocalBounds().toFloat();
    }
    void releaseOpenGLResources() override {}
private:
    EqualizerEditorComponent editor;
};

class EqualizerNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node& node,
            const NodeEditorContext& context) const override {
        ignoreUnused(node);
        return std::make_unique<EqualizerNodeEditor>(context);
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
    factories.emplace_back(NodeKind::Equalizer, std::make_unique<EqualizerNodeEditorFactory>());
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
