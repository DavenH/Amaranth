#include <Audio/CycleDsp/CycleDelay.h>
#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Delay/DelayNodeEditor.h"
#include "Nodes/Delay/DelayPreviewPainter.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/NodePropertyControlBinding.h"

namespace CycleV2 {

namespace {

constexpr int kPreviewHeight = 150;
constexpr int kPropertyStart = 210;

String formatBeats(double value) {
    const String beats = formatPropertyReal(CycleDsp::delayBeats((float) value, 4));
    return beats + (beats == "1" ? " beat" : " beats");
}

std::optional<double> parseBeats(const String& text) {
    const auto beats = parsePropertyNumber(text, "beats");
    if (!beats.has_value() || *beats < 0.09 || *beats > 4.0) {
        return std::nullopt;
    }
    return CycleDsp::delayUnitValueForBeats(*beats, 4);
}

String formatPanCycle(double value) {
    return String(CycleDsp::delaySpinIterations(value)) + String::fromUTF8("×");
}

std::optional<double> parsePanCycle(String text) {
    text = text.trimCharactersAtEnd(String("xX") + String::fromUTF8("×")).trimEnd();
    const auto iterations = parsePropertyNumber(text);
    if (!iterations.has_value() || *iterations < 1.0 || *iterations > 12.0
            || !approximatelyEqual(*iterations, (double) roundToInt(*iterations))) {
        return std::nullopt;
    }
    return CycleDsp::delaySpinUnitValueForIterations(roundToInt(*iterations));
}

void configurePercentage(NodePropertySliderRow& row, float defaultValue) {
    row.slider.setRange(0.0, 1.0, 0.00001);
    row.configureValuePresentation(
            formatPropertyPercentage,
            parsePropertyPercentage,
            defaultValue,
            0.01,
            0.001,
            "Percentage. Shift-drag or Shift-arrow for fine adjustment.");
}

class DelayEditorComponent final : public Component {
public:
    DelayEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse) :
            commands     (commandsToUse)
        ,   presentation (presentationToUse)
        ,   time         (*this, commands, "time", "Time")
        ,   feedback     (*this, commands, "feedback", "Feedback")
        ,   panAmount    (*this, commands, "spin", "Pan Amount")
        ,   panCycle     (*this, commands, "spinIters", "Pan Cycle")
        ,   wet          (*this, commands, "wet", "Wet") {
        configureHeader();
        configureControls();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        enabled.setToggleState(parameters.boolValue("enabled", true), dontSendNotification);
        time.bind(node.id, parameters.floatValue("time", 0.5f));
        feedback.bind(node.id, parameters.floatValue("feedback", 0.5f));
        panAmount.bind(node.id, parameters.floatValue("spin", 0.5f));
        panCycle.bind(node.id, parameters.floatValue("spinIters", 0.f));
        wet.bind(node.id, parameters.floatValue("wet", 0.5f));
        repaint();
    }

    void paint(Graphics& graphics) override {
        graphics.fillAll(Colour(0xff11151b));
        graphics.setColour(Colour(0xff2b3340));
        graphics.drawRoundedRectangle(
                getLocalBounds().toFloat().reduced(0.5f),
                CanvasChromeMetrics::panelCornerRadius,
                CanvasChromeMetrics::restingBorderWidth);
        graphics.setColour(Colour(0xffeef2f6));
        graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize));
        graphics.drawText("DELAY", 18, 10, getWidth() - 80, 28, Justification::centredLeft);
        if (node.id.isNotEmpty()) {
            DelayPreviewPainter().paint(graphics, previewBounds(), node, 1.f);
        }
    }

    void resized() override {
        close.setBounds(getWidth() - 42, 9, 28, 28);
        enabled.setBounds(getWidth() - 142, 12, 88, 24);
        Rectangle<int> rows(18, kPropertyStart, getWidth() - 36, getHeight() - kPropertyStart);
        for (auto* row : propertyRows()) {
            row->setBounds(rows.removeFromTop(PropertyControlMetrics::compactRowHeight));
            rows.removeFromTop(PropertyControlMetrics::rowGap);
        }
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", "DELAY");
        state->setProperty("enabled", enabled.getToggleState());
        Array<var> controls;
        for (const auto* row : propertyRows()) {
            var value = propertySliderRowAutomationState(*row);
            value.getDynamicObject()->setProperty("id", row->parameterId());
            value.getDynamicObject()->setProperty("value", row->slider.getValue());
            value.getDynamicObject()->setProperty("readout", row->valueText());
            controls.add(value);
        }
        state->setProperty("controls", controls);
        return state;
    }

private:
    void configureHeader() {
        close.setButtonText(String::fromUTF8("×"));
        close.setComponentID("delayEditor.close");
        close.setTooltip("Close Delay editor");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] {
            presentation.closeNodeEditor();
        };
        enabled.setButtonText("Enabled");
        enabled.setComponentID("delayEditor.enabled");
        enabled.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id,
                    "enabled",
                    "Enabled",
                    enabled.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(close);
        addAndMakeVisible(enabled);
    }

    void configureControls() {
        configureRows();
        configureTime();
        configurePercentage(feedback, 0.5f);
        configurePercentage(panAmount, 0.5f);
        configurePercentage(wet, 0.5f);
        configurePanCycle();
    }

    void configureRows() {
        for (auto* row : propertyRows()) {
            row->setCompactLayout(true);
            row->slider.setComponentID("delayEditor." + row->parameterId());
            row->value.setComponentID("delayEditor." + row->parameterId() + ".value");
            row->onPreviewValue = [this, row](float value) {
                updateLocalParameter(row->parameterId(), value);
            };
        }
    }

    void configureTime() {
        time.slider.setRange(0.0, 1.0, 0.00001);
        time.configureValuePresentation(
                formatBeats,
                parseBeats,
                0.5,
                0.01,
                0.001,
                "Delay time in beats. Shift-drag for fine adjustment; double-click for 1 beat.");
        time.slider.setValueSnapper([this](double value, Slider::DragMode dragMode) {
            return dragMode == Slider::notDragging
                    ? value
                    : CycleDsp::delaySnappedUnitValue(
                            (float) value,
                            4,
                            (float) time.slider.getWidth());
        });
        time.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
            const double beats = CycleDsp::delayBeats((float) current, 4);
            const double step = fine ? 0.05 : 0.25;
            return CycleDsp::delayUnitValueForBeats(
                    jlimit(0.09, 4.0, beats + (increase ? step : -step)),
                    4);
        });
        time.slider.setLandmarks({
                { CycleDsp::delayUnitValueForBeats(0.5, 4), "0.5" },
                { CycleDsp::delayUnitValueForBeats(1.0, 4), "1" },
                { CycleDsp::delayUnitValueForBeats(2.0, 4), "2" },
                { CycleDsp::delayUnitValueForBeats(3.0, 4), "3" },
                { CycleDsp::delayUnitValueForBeats(4.0, 4), "4" }
        });
    }

    void configurePanCycle() {
        panCycle.slider.setRange(0.0, 1.0, 0.00001);
        panCycle.configureValuePresentation(
                formatPanCycle,
                parsePanCycle,
                0.0,
                1.0 / 11.0,
                1.0 / 11.0,
                "One complete stereo pan cycle spans this many delay intervals.");
        panCycle.slider.setValueSnapper([](double value, Slider::DragMode) {
            return CycleDsp::delaySpinUnitValueForIterations(
                    CycleDsp::delaySpinIterations(value));
        });
        panCycle.slider.setKeyboardStepper([](double current, bool increase, bool) {
            return CycleDsp::delaySpinUnitValueForIterations(
                    CycleDsp::delaySpinIterations(current) + (increase ? 1 : -1));
        });
        std::vector<PrecisionSlider::Landmark> panCycleLandmarks;
        for (int iterations = 1; iterations <= 12; ++iterations) {
            panCycleLandmarks.push_back({
                    CycleDsp::delaySpinUnitValueForIterations(iterations),
                    String(iterations)
            });
        }
        panCycle.slider.setLandmarks(std::move(panCycleLandmarks));
    }

    void updateLocalParameter(const String& id, float value) {
        const auto* definition = NodeDefinitionRegistry::instance().findParameter(
                NodeKind::Delay,
                id);
        const String normalized = definition != nullptr
                ? definition->normalized(String(value, 6))
                : String(value, 6);
        for (auto& parameter : node.parameters) {
            if (parameter.id == id) {
                parameter.value = normalized;
                break;
            }
        }
        repaint();
    }

    std::array<NodePropertySliderRow*, 5> propertyRows() {
        return { &time, &feedback, &panAmount, &panCycle, &wet };
    }

    std::array<const NodePropertySliderRow*, 5> propertyRows() const {
        return { &time, &feedback, &panAmount, &panCycle, &wet };
    }

    Rectangle<float> previewBounds() const {
        return { 18.f, 52.f, (float) getWidth() - 36.f, (float) kPreviewHeight };
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    Node node;
    TextButton close;
    ToggleButton enabled;
    NodePropertySliderRow time;
    NodePropertySliderRow feedback;
    NodePropertySliderRow panAmount;
    NodePropertySliderRow panCycle;
    NodePropertySliderRow wet;
};

class DelayNodeEditor final : public NodeEditor {
public:
    explicit DelayNodeEditor(const NodeEditorContext& context) :
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
    DelayEditorComponent editor;
};

class DelayNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<DelayNodeEditor>(context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createDelayNodeEditorFactory() {
    return std::make_unique<DelayNodeEditorFactory>();
}

}
