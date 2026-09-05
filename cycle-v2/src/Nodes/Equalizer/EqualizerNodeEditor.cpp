#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <array>
#include <cmath>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Equalizer/EqualizerNodeEditor.h"
#include "Nodes/Equalizer/EqualizerPreviewPainter.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"
#include "UI/EffectEnableButton.h"
#include "UI/Editors/NodePropertyControlBinding.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

namespace {

constexpr int kPreviewHeight = 150;
constexpr int kPropertyStart = 242;
constexpr int kColumnGap = 46;

String formatGain(double value) {
    const float decibels = CycleDsp::equalizerGainDecibels((float) value);
    return (decibels > 0.f ? "+" : "") + formatPropertyReal(decibels) + " dB";
}

std::optional<double> parseGain(const String& text) {
    const auto decibels = parsePropertyNumber(text, "dB");
    if (!decibels.has_value() || *decibels < -30.0 || *decibels > 30.0) {
        return std::nullopt;
    }
    return CycleDsp::equalizerGainUnitValue((float) *decibels);
}

String formatFrequency(double value) {
    return formatPropertyFrequency(CycleDsp::equalizerFrequency((float) value));
}

std::optional<double> parseFrequency(const String& text) {
    const auto frequency = parsePropertyFrequency(text, 20.0, 20000.0);
    if (!frequency.has_value()) {
        return std::nullopt;
    }
    return CycleDsp::equalizerFrequencyUnitValue((float) *frequency);
}

String frequencyHelp(const String& id) {
    if (id.startsWith("band1")) {
        return "Low-shelf corner frequency. Shift-drag for fine adjustment.";
    }
    if (id.startsWith("band5")) {
        return "High-shelf corner frequency. Shift-drag for fine adjustment.";
    }
    return "Fixed-bandwidth centre frequency. Shift-drag for fine adjustment.";
}

std::vector<PrecisionSlider::Landmark> frequencyLandmarks() {
    constexpr float frequencies[] {
            60.f, 120.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f
    };
    constexpr const char* labels[] {
            "60", "120", "250", "500", "1k", "2k", "4k", "8k", "16k"
    };
    std::vector<PrecisionSlider::Landmark> result;
    for (size_t index = 0; index < std::size(frequencies); ++index) {
        result.push_back({
                CycleDsp::equalizerFrequencyUnitValue(frequencies[index]),
                labels[index]
        });
    }
    return result;
}

struct EqualizerControl {
    EqualizerControl(
            Component& owner,
            NodeEditorCommands& commands,
            String idToUse,
            String nameToUse,
            float defaultValueToUse) :
            id            (std::move(idToUse))
        ,   name          (std::move(nameToUse))
        ,   defaultValue  (defaultValueToUse)
        ,   row           (owner, commands, id, name) {}

    String id;
    String name;
    float defaultValue {};
    NodePropertySliderRow row;
};

class EqualizerEditorComponent final : public Component {
public:
    EqualizerEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse) :
            commands     (commandsToUse)
        ,   presentation (presentationToUse) {
        configureHeader();
        createControls();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        enabled.setToggleState(parameters.boolValue("enabled", true), dontSendNotification);
        for (auto& control : controls) {
            control->row.bind(
                    node.id,
                    parameters.floatValue(control->id, control->defaultValue));
        }
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
        const auto header = fullEditorHeaderLayout(getLocalBounds(), true);
        graphics.drawText("EQUALIZER", header.title, Justification::centredLeft);
        paintResponse(graphics);
    }

    void resized() override {
        const auto header = fullEditorHeaderLayout(getLocalBounds(), true);
        close.setBounds(header.close);
        enabled.setBounds(header.enabled);
        const int columnWidth = (getWidth() - 76 - kColumnGap) / 2;
        const int frequencyX = 38 + columnWidth + kColumnGap;
        gainHeader.setBounds(38, kPropertyStart - 22, columnWidth, 18);
        frequencyHeader.setBounds(frequencyX, kPropertyStart - 22, columnWidth, 18);
        for (size_t band = 0; band < controls.size() / 2; ++band) {
            const int y = kPropertyStart
                    + (int) band * (PropertyControlMetrics::compactRowHeight
                            + PropertyControlMetrics::rowGap);
            controls[band * 2]->row.setBounds({
                    38, y, columnWidth, PropertyControlMetrics::compactRowHeight
            });
            controls[band * 2 + 1]->row.setBounds({
                    frequencyX, y, columnWidth, PropertyControlMetrics::compactRowHeight
            });
        }
    }

    void mouseDown(const MouseEvent& event) override {
        beginGraphDrag(event.position);
    }

    void mouseDrag(const MouseEvent& event) override {
        updateGraphDrag(event.position);
    }

    void mouseUp(const MouseEvent&) override {
        endGraphDrag();
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", "EQUALIZER");
        state->setProperty("enabled", enabled.getToggleState());
        Array<var> values;
        for (const auto& control : controls) {
            var value = propertySliderRowAutomationState(control->row);
            value.getDynamicObject()->setProperty("id", control->id);
            value.getDynamicObject()->setProperty("value", control->row.slider.getValue());
            value.getDynamicObject()->setProperty("readout", control->row.valueText());
            values.add(value);
        }
        state->setProperty("controls", values);
        return state;
    }

private:
    void configureHeader() {
        close.setButtonText(String::fromUTF8("×"));
        close.setComponentID("equalizerEditor.close");
        close.setTooltip("Close Equalizer editor");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] {
            presentation.closeNodeEditor();
        };
        enabled.setComponentID("equalizerEditor.enabled");
        enabled.onClick = [this] {
            commands.setNodeParameterValue(
                    node.id,
                    "enabled",
                    "Enabled",
                    enabled.getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible(close);
        addAndMakeVisible(enabled);
        stylePropertyLabel(gainHeader, "GAIN");
        stylePropertyLabel(frequencyHeader, "FREQUENCY");
        gainHeader.setJustificationType(Justification::centredLeft);
        frequencyHeader.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(gainHeader);
        addAndMakeVisible(frequencyHeader);
    }

    void createControls() {
        constexpr std::array<float, CycleDsp::equalizerBandCount> frequencies {
                60.f, 250.f, 1200.f, 4000.f, 8000.f
        };
        for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
            const String prefix = "band" + String(band + 1);
            addControl(prefix + "Gain", "Band " + String(band + 1) + " Gain", 0.5f, true);
            addControl(
                    prefix + "Frequency",
                    "Band " + String(band + 1) + " Frequency",
                    CycleDsp::equalizerFrequencyUnitValue(frequencies[(size_t) band]),
                    false);
        }
    }

    void addControl(
            const String& id,
            const String& name,
            float defaultValue,
            bool gain) {
        auto control = std::make_unique<EqualizerControl>(
                *this,
                commands,
                id,
                name,
                defaultValue);
        EqualizerControl* raw = control.get();
        configureControl(*raw, gain);
        controls.push_back(std::move(control));
    }

    void configureControl(EqualizerControl& control, bool gain) {
        NodePropertySliderRow& row = control.row;
        row.setCompactLayout(true);
        row.slider.setRange(0.0, 1.0, 0.00001);
        row.slider.setComponentID("equalizerEditor." + control.id);
        row.value.setComponentID("equalizerEditor." + control.id + ".value");
        row.label.setText(
                gain ? control.id.substring(4, 5) : "",
                dontSendNotification);
        if (gain) {
            configureGainControl(control);
        } else {
            configureFrequencyControl(control);
        }
        EqualizerControl* target = &control;
        row.onPreviewValue = [this, target](float value) {
            updateLocalParameter(target->id, value);
        };
    }

    void configureGainControl(EqualizerControl& control) {
        NodePropertySliderRow& row = control.row;
        row.configureValuePresentation(
                formatGain,
                parseGain,
                control.defaultValue,
                0.01,
                0.001,
                "Gain in dB. Shift-drag for fine adjustment; double-click for 0 dB.");
        row.slider.setValueSnapper([slider = &row.slider](
                double value,
                Slider::DragMode dragMode) {
            return dragMode == Slider::absoluteDrag
                    ? CycleDsp::equalizerGainSnappedUnitValue(
                            (float) value,
                            (float) slider->getWidth())
                    : value;
        });
        row.slider.setLandmarks({ { 0.5, "0" } });
    }

    void configureFrequencyControl(EqualizerControl& control) {
        control.row.configureValuePresentation(
                formatFrequency,
                parseFrequency,
                control.defaultValue,
                0.01,
                0.001,
                frequencyHelp(control.id));
        control.row.slider.setLandmarks(frequencyLandmarks());
    }

    void paintResponse(Graphics& graphics) {
        if (node.id.isEmpty()) {
            return;
        }
        const Rectangle<float> response = responseBounds();
        graphics.setColour(EffectPlotPalette::forEnabledState(
                EffectPlotPalette::insetBackground,
                enabled.getToggleState()));
        graphics.fillRoundedRectangle(response, CanvasChromeMetrics::insetCornerRadius);
        EqualizerPreviewPainter().paint(graphics, response.reduced(12.f, 9.f), node, true);
    }

    void beginGraphDrag(Point<float> position) {
        if (!equalizerGraphArea().contains(position)) {
            return;
        }
        constexpr float markerHitRadius = 36.f;
        float nearestDistance = markerHitRadius;
        for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
            const float distance = EqualizerPreviewPainter()
                    .bandControlPoint(equalizerGraphArea(), node, band)
                    .getDistanceFrom(position);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                draggedBand = band;
            }
        }
        if (draggedBand < 0) {
            return;
        }
        EqualizerControl& gain = *controls[(size_t) draggedBand * 2];
        EqualizerControl& frequency = *controls[(size_t) draggedBand * 2 + 1];
        commands.beginNodeParameterPairEdit(
                node.id,
                gain.id,
                gain.name,
                (float) gain.row.slider.getValue(),
                frequency.id,
                frequency.name,
                (float) frequency.row.slider.getValue());
    }

    void updateGraphDrag(Point<float> position) {
        if (draggedBand < 0) {
            return;
        }
        const Rectangle<float> graph = equalizerGraphArea();
        const float frequencyPosition = jlimit(
                0.f, 1.f, (position.x - graph.getX()) / graph.getWidth());
        const float gainValue = jlimit(
                0.f, 1.f, (graph.getBottom() - position.y) / graph.getHeight());
        const float frequency = 40.f * std::pow(400.f, frequencyPosition);
        const float frequencyValue = CycleDsp::equalizerFrequencyUnitValue(frequency);
        EqualizerControl& gain = *controls[(size_t) draggedBand * 2];
        EqualizerControl& frequencyControl = *controls[(size_t) draggedBand * 2 + 1];
        setGraphControlValue(gain, gainValue);
        setGraphControlValue(frequencyControl, frequencyValue);
        commands.updateNodeParameterPairEditValues(gainValue, frequencyValue);
        repaint();
    }

    void endGraphDrag() {
        if (draggedBand < 0) {
            return;
        }
        draggedBand = -1;
        commands.endNodeParameterEdit();
    }

    void setGraphControlValue(EqualizerControl& control, float value) {
        control.row.slider.setValue(value, dontSendNotification);
        control.row.refreshValueText();
        updateLocalParameter(control.id, value);
    }

    void updateLocalParameter(const String& id, float value) {
        const auto* definition = NodeDefinitionRegistry::instance().findParameter(
                NodeKind::Equalizer,
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

    Rectangle<float> responseBounds() const {
        return { 18.f, 52.f, (float) getWidth() - 36.f, (float) kPreviewHeight };
    }

    Rectangle<float> equalizerGraphArea() const {
        return responseBounds().reduced(12.f, 9.f);
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    Node node;
    TextButton close;
    EffectEnableButton enabled;
    Label gainHeader;
    Label frequencyHeader;
    int draggedBand { -1 };
    std::vector<std::unique_ptr<EqualizerControl>> controls;
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
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<EqualizerNodeEditor>(context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createEqualizerNodeEditorFactory() {
    return std::make_unique<EqualizerNodeEditorFactory>();
}

}
