#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>
#include <limits>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Reverb/ReverbNodeEditor.h"
#include "UI/Editors/NodePropertyControlBinding.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

namespace {

constexpr int kPreviewHeight = 150;
constexpr int kPropertyStart = 210;
constexpr double kReferenceSampleRate = 44100.0;

String formatSize(double value) {
    return formatPropertyReal(
            CycleDsp::reverbKernelSeconds((float) value, kReferenceSampleRate)) + " s";
}

std::optional<double> parseSize(const String& text) {
    const auto seconds = parsePropertyNumber(text, "s");
    if (!seconds.has_value()) {
        return std::nullopt;
    }
    int closestStep {};
    double closestDistance = std::numeric_limits<double>::max();
    for (int step = 0; step < CycleDsp::reverbSizeStepCount; ++step) {
        const float value = CycleDsp::reverbSizeUnitValueForStep(step);
        const double distance = std::abs(
                CycleDsp::reverbKernelSeconds(value, kReferenceSampleRate) - *seconds);
        if (distance < closestDistance) {
            closestStep = step;
            closestDistance = distance;
        }
    }
    const float closestValue = CycleDsp::reverbSizeUnitValueForStep(closestStep);
    const double closestSeconds = CycleDsp::reverbKernelSeconds(
            closestValue,
            kReferenceSampleRate);
    return std::abs(*seconds - closestSeconds) <= 0.005
            ? std::optional<double>(closestValue)
            : std::nullopt;
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

class ReverbEditorComponent final : public Component {
public:
    ReverbEditorComponent(
            NodeEditorCommands& commandsToUse,
            NodeEditorPresentation& presentationToUse,
            NodeEditorResources& resourcesToUse) :
            commands     (commandsToUse)
        ,   presentation (presentationToUse)
        ,   resources    (resourcesToUse)
        ,   size         (*this, commands, "size", "Size")
        ,   damping      (*this, commands, "damp", "Damping")
        ,   width        (*this, commands, "width", "Width")
        ,   highPass     (*this, commands, "highPass", "High Pass")
        ,   wet          (*this, commands, "wet", "Wet") {
        configureHeader();
        configureControls();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        enabled.setToggleState(parameters.boolValue("enabled", true), dontSendNotification);
        size.bind(node.id, parameters.floatValue("size", 0.5f));
        damping.bind(node.id, parameters.floatValue("damp", 0.2f));
        width.bind(node.id, parameters.floatValue("width", 1.f));
        highPass.bind(node.id, parameters.floatValue("highPass", 0.05f));
        wet.bind(node.id, parameters.floatValue("wet", 0.4f));
        repaint();
    }

    void paint(Graphics& graphics) override {
        graphics.fillAll(Colour(0xff11151b));
        graphics.setColour(Colour(0xff2b3340));
        graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 10.f, 1.f);
        graphics.setColour(Colour(0xffeef2f6));
        graphics.setFont(FontOptions(18.f));
        graphics.drawText("REVERB", 18, 10, getWidth() - 80, 28, Justification::centredLeft);
        if (node.id.isNotEmpty()) {
            const Rectangle<float> response = previewBounds();
            graphics.setColour(EffectPlotPalette::insetBackground);
            graphics.fillRoundedRectangle(response, 6.f);
            resources.paintNodePreview(graphics, node, response.reduced(5.f));
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
        state->setProperty("kind", "REVERB");
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
        close.setComponentID("reverbEditor.close");
        close.setTooltip("Close Reverb editor");
        close.setWantsKeyboardFocus(true);
        close.onClick = [this] {
            presentation.closeNodeEditor();
        };
        enabled.setButtonText("Enabled");
        enabled.setComponentID("reverbEditor.enabled");
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
        configureSize();
        configurePercentage(damping, 0.2f);
        configurePercentage(width, 1.f);
        configurePercentage(highPass, 0.05f);
        configurePercentage(wet, 0.4f);
    }

    void configureRows() {
        for (auto* row : propertyRows()) {
            row->setCompactLayout(true);
            row->slider.setComponentID("reverbEditor." + row->parameterId());
            row->value.setComponentID("reverbEditor." + row->parameterId() + ".value");
            row->onPreviewValue = [this, row](float value) {
                updateLocalParameter(row->parameterId(), value);
            };
        }
    }

    void configureSize() {
        size.slider.setRange(0.0, 1.0, 0.00001);
        size.configureValuePresentation(
                formatSize,
                parseSize,
                0.5,
                1.0 / (CycleDsp::reverbSizeStepCount - 1),
                1.0 / (CycleDsp::reverbSizeStepCount - 1),
                "Reverb kernel duration at 44.1 kHz; seven discrete sizes.");
        size.slider.setValueSnapper([](double value, Slider::DragMode) {
            const int step = jlimit(
                    0,
                    CycleDsp::reverbSizeStepCount - 1,
                    roundToInt(value * (CycleDsp::reverbSizeStepCount - 1)));
            return CycleDsp::reverbSizeUnitValueForStep(step);
        });
        size.slider.setKeyboardStepper([](double current, bool increase, bool) {
            const int step = roundToInt(current * (CycleDsp::reverbSizeStepCount - 1));
            return CycleDsp::reverbSizeUnitValueForStep(step + (increase ? 1 : -1));
        });
        std::vector<PrecisionSlider::Landmark> sizeLandmarks;
        for (int step = 0; step < CycleDsp::reverbSizeStepCount; ++step) {
            const float value = CycleDsp::reverbSizeUnitValueForStep(step);
            const double seconds = CycleDsp::reverbKernelSeconds(value, kReferenceSampleRate);
            sizeLandmarks.push_back({ value, formatPropertyReal(seconds) });
        }
        size.slider.setLandmarks(std::move(sizeLandmarks));
    }

    void updateLocalParameter(const String& id, float value) {
        const auto* definition = NodeDefinitionRegistry::instance().findParameter(
                NodeKind::Reverb,
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
        return { &size, &damping, &width, &highPass, &wet };
    }

    std::array<const NodePropertySliderRow*, 5> propertyRows() const {
        return { &size, &damping, &width, &highPass, &wet };
    }

    Rectangle<float> previewBounds() const {
        return { 18.f, 52.f, (float) getWidth() - 36.f, (float) kPreviewHeight };
    }

    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
    NodeEditorResources& resources;
    Node node;
    TextButton close;
    ToggleButton enabled;
    NodePropertySliderRow size;
    NodePropertySliderRow damping;
    NodePropertySliderRow width;
    NodePropertySliderRow highPass;
    NodePropertySliderRow wet;
};

class ReverbNodeEditor final : public NodeEditor {
public:
    explicit ReverbNodeEditor(const NodeEditorContext& context) :
            editor(context.commands, context.presentation, context.resources) {}

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
    ReverbEditorComponent editor;
};

class ReverbNodeEditorFactory final : public NodeEditorFactory {
public:
    std::unique_ptr<NodeEditor> create(
            const Node&,
            const NodeEditorContext& context) const override {
        return std::make_unique<ReverbNodeEditor>(context);
    }
};

}

std::unique_ptr<NodeEditorFactory> createReverbNodeEditorFactory() {
    return std::make_unique<ReverbNodeEditorFactory>();
}

}
