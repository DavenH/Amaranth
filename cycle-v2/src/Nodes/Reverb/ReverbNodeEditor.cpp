#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>
#include <limits>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Reverb/ReverbNodeEditor.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/NodePropertyControlBinding.h"
#include "UI/ExpandedEditorChrome.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

namespace {

constexpr int kPreviewHeight = 150;
constexpr int kPropertyStart = 204;
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
        ,   chrome       (*this, "REVERB", "reverbEditor",
                    [this] { presentation.closeNodeEditor(); },
                    [this](bool enabled) {
                        commands.setNodeParameterValue(
                                node.id, "enabled", "Enabled", enabled ? 1.f : 0.f);
                    })
        ,   size         (*this, commands, "size", "Size")
        ,   damping      (*this, commands, "damp", "Damping")
        ,   width        (*this, commands, "width", "Width")
        ,   highPass     (*this, commands, "highPass", "High Pass")
        ,   wet          (*this, commands, "wet", "Wet") {
        addAndMakeVisible(spaceGroup);
        addAndMakeVisible(toneOutputGroup);
        configureControls();
    }

    void setNode(const Node& nextNode) {
        node = nextNode;
        const NodeParameterMap parameters(node);
        chrome.setEnabled(parameters.boolValue("enabled", true));
        size.bind(node.id, parameters.floatValue("size", 0.5f));
        damping.bind(node.id, parameters.floatValue("damp", 0.2f));
        width.bind(node.id, parameters.floatValue("width", 1.f));
        highPass.bind(node.id, parameters.floatValue("highPass", 0.05f));
        wet.bind(node.id, parameters.floatValue("wet", 0.4f));
        repaint();
    }

    void paint(Graphics& graphics) override {
        chrome.paint(graphics);
        if (node.id.isNotEmpty()) {
            const Rectangle<float> response = previewBounds();
            graphics.setColour(EffectPlotPalette::insetBackground);
            graphics.fillRoundedRectangle(response, CanvasChromeMetrics::insetCornerRadius);
            resources.paintNodePreview(graphics, node, response.reduced(5.f));
        }
    }

    void resized() override {
        chrome.resized();
        Rectangle<int> rows(18, kPropertyStart, getWidth() - 36, getHeight() - kPropertyStart);
        spaceGroup.setBounds(rows.removeFromTop(PropertyControlMetrics::groupLabelHeight));
        layoutCompactRow(size, rows);
        layoutCompactRow(damping, rows);
        layoutCompactRow(width, rows);
        toneOutputGroup.setBounds(
                rows.removeFromTop(PropertyControlMetrics::groupLabelHeight));
        layoutCompactRow(highPass, rows);
        wet.setBounds(rows.removeFromTop(PropertyControlMetrics::compactRowHeight));
    }

    var automationState() const {
        auto* state = new DynamicObject();
        state->setProperty("kind", "REVERB");
        state->setProperty("enabled", chrome.isEnabled());
        state->setProperty("spaceGroup", propertyGroupLabelAutomationState(spaceGroup));
        state->setProperty(
                "toneOutputGroup",
                propertyGroupLabelAutomationState(toneOutputGroup));
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
    void configureControls() {
        configureRows();
        configureSize();
        configurePercentage(damping, 0.2f);
        configurePercentage(width, 1.f);
        configurePercentage(highPass, 0.05f);
        configurePercentage(wet, 0.4f);
    }

    static void layoutCompactRow(
            NodePropertySliderRow& row,
            Rectangle<int>& available) {
        row.setBounds(available.removeFromTop(PropertyControlMetrics::compactRowHeight));
        available.removeFromTop(PropertyControlMetrics::rowGap);
    }

    void configureRows() {
        for (auto* row : propertyRows()) {
            row->setCompactLayout(true);
            row->slider.setComponentID("reverbEditor." + row->parameterId());
            row->value.setComponentID("reverbEditor." + row->parameterId() + ".value");
            row->mirrorPreviewInto(node, NodeKind::Reverb);
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
                "Reverb kernel duration at 44.1 kHz. Seven power-of-two sizes.");
        size.slider.setValueSnapper([](double value, Slider::DragMode dragMode) {
            return dragMode == Slider::notDragging
                    ? value
                    : CycleDsp::reverbSizeSnappedUnitValue((float) value);
        });
        size.slider.setKeyboardStepper([](double current, bool increase, bool) {
            return CycleDsp::reverbSizeSteppedUnitValue((float) current, increase);
        });
        std::vector<PrecisionSlider::Landmark> sizeLandmarks;
        for (int step = 0; step < CycleDsp::reverbSizeStepCount; ++step) {
            const float value = CycleDsp::reverbSizeUnitValueForStep(step);
            const double seconds = CycleDsp::reverbKernelSeconds(value, kReferenceSampleRate);
            sizeLandmarks.push_back({ value, formatPropertyReal(seconds) });
        }
        size.slider.setLandmarks(std::move(sizeLandmarks));
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
    ExpandedEditorChrome chrome;
    PropertyGroupLabel spaceGroup { "Space" };
    PropertyGroupLabel toneOutputGroup { "Tone / output" };
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
