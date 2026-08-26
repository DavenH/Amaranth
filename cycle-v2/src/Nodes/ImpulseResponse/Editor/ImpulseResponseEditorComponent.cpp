#include "Nodes/ImpulseResponse/Editor/ImpulseResponseEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

#include <Audio/CycleDsp/IrModel.h>

#include <cmath>
#include <cstdlib>

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 348.f;
constexpr int kValueWidth = 72;

std::optional<double> parseNumber(String text) {
    text = text.trim();
    if (text.isEmpty()) {
        return std::nullopt;
    }
    const char* start = text.toRawUTF8();
    char* end {};
    const double value = std::strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

String formatSize(double value) {
    return String(CycleDsp::irImpulseLength(value)) + " smp";
}

std::optional<double> parseSize(String text) {
    text = text.trim();
    for (const String& suffix : { String("samples"), String("sample"), String("smp") }) {
        if (text.endsWithIgnoreCase(suffix)) {
            text = text.dropLastCharacters(suffix.length()).trimEnd();
            break;
        }
    }
    const auto parsed = parseNumber(text);
    if (!parsed.has_value()) {
        return std::nullopt;
    }
    const int length = roundToInt(*parsed);
    if (!approximatelyEqual(*parsed, (double) length)
            || length < 128 || length > 16384
            || (length & (length - 1)) != 0) {
        return std::nullopt;
    }
    return CycleDsp::irImpulseLengthValue(length);
}

String formatPostGain(double value) {
    const float decibels = CycleDsp::irPostGainDecibels(value);
    return (decibels > 0.f ? "+" : "") + String(decibels, 1) + " dB";
}

std::optional<double> parsePostGain(String text) {
    text = text.trim();
    if (text.endsWithIgnoreCase("db")) {
        text = text.dropLastCharacters(2).trimEnd();
    }
    const auto decibels = parseNumber(text);
    if (!decibels.has_value()) {
        return std::nullopt;
    }
    const float minimum = CycleDsp::irPostGainDecibels(0.0);
    const float maximum = CycleDsp::irPostGainDecibels(1.0);
    if (*decibels < minimum || *decibels > maximum) {
        return std::nullopt;
    }
    return CycleDsp::irPostGainValueForDecibels((float) *decibels);
}

String formatHighPass(double value) {
    return String(CycleDsp::irPrefilterAmount(value) * 100.f, 1) + "% Nyq";
}

std::optional<double> parseHighPass(String text) {
    text = text.trim();
    if (text.endsWithIgnoreCase("nyq")) {
        text = text.dropLastCharacters(3).trimEnd();
    }
    if (text.endsWithChar('%')) {
        text = text.dropLastCharacters(1).trimEnd();
    }
    const auto percentage = parseNumber(text);
    if (!percentage.has_value() || *percentage < 0.0 || *percentage > 100.0) {
        return std::nullopt;
    }
    return CycleDsp::irPrefilterValueForAmount((float) (*percentage / 100.0));
}

void configureSizeControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 1.0 / 7.0);
    control.slider.setComponentID("irEditor.size");
    control.value.setComponentID("irEditor.size.value");
    control.configureValuePresentation(
            formatSize,
            parseSize,
            CycleDsp::irImpulseLengthValue(1024),
            1.0 / 7.0,
            1.0 / 7.0,
            "Impulse length in samples: eight power-of-two sizes from 128 to 16384.");
}

void configurePostGainControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 0.00001);
    control.slider.setComponentID("irEditor.postGain");
    control.value.setComponentID("irEditor.postGain.value");
    control.configureValuePresentation(
            formatPostGain,
            parsePostGain,
            0.5,
            0.01,
            0.001,
            "Output gain in decibels. Shift-drag for fine adjustment; double-click for 0 dB.");
    control.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
        const float decibels = CycleDsp::irPostGainDecibels(current);
        const float step = fine ? 0.1f : 1.f;
        return CycleDsp::irPostGainValueForDecibels(
                decibels + (increase ? step : -step));
    });
}

void configureHighPassControl(LabeledParameterSlider& control) {
    control.slider.setRange(0.0, 1.0, 0.00001);
    control.slider.setComponentID("irEditor.highPass");
    control.value.setComponentID("irEditor.highPass.value");
    control.configureValuePresentation(
            formatHighPass,
            parseHighPass,
            0.0,
            0.01,
            0.001,
            "High-pass cutoff as a percentage of Nyquist. Shift-drag for fine adjustment.");
    control.slider.setKeyboardStepper([](double current, bool increase, bool fine) {
        const float amount = CycleDsp::irPrefilterAmount(current);
        const double step = fine ? 0.001 : 0.01;
        return CycleDsp::irPrefilterValueForAmount((float) jlimit(
                0.0,
                1.0,
                amount + (increase ? step : -step)));
    });
}

Array<var> sampleLandmarks(int sampleCount) {
    Array<var> result;
    for (int numerator : { 0, 1, 2, 3, 4 }) {
        auto* landmark = new DynamicObject();
        landmark->setProperty("fraction", numerator / 4.0);
        landmark->setProperty("sample", roundToInt(sampleCount * numerator / 4.0));
        result.add(var(landmark));
    }
    return result;
}

}

struct ImpulseResponseEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enabled")
        ,   size        (owner, "Size")
        ,   postGain    (owner, "Post Gain")
        ,   highPass    (owner, "High Pass") {}

    ParameterToggle enabled;
    LabeledParameterSlider size;
    LabeledParameterSlider postGain;
    LabeledParameterSlider highPass;
};

ImpulseResponseEditorComponent::ImpulseResponseEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    configureSizeControl(impl->size);
    configurePostGainControl(impl->postGain);
    configureHighPassControl(impl->highPass);
    bindDiscreteControl(impl->enabled);
    bindContinuousControls({ &impl->size, &impl->postGain, &impl->highPass });
}

ImpulseResponseEditorComponent::~ImpulseResponseEditorComponent() = default;

Rectangle<float> ImpulseResponseEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(kControlRailWidth);
}

Rectangle<float> ImpulseResponseEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(kControlRailWidth);
    return bounds.reduced(12.f, 26.f);
}

void ImpulseResponseEditorComponent::paintEditor(Graphics& graphics) {
    const Rectangle<float> panel = editorPanelBounds();
    const int sampleCount = CycleDsp::irImpulseLength(impl->size.slider.getValue());
    graphics.setColour(Colour(0xff8793a1));
    graphics.setFont(FontOptions(10.f));
    for (int numerator : { 0, 1, 2, 3, 4 }) {
        const float fraction = numerator / 4.f;
        const float x = panel.getX() + panel.getWidth() * fraction;
        graphics.drawVerticalLine(roundToInt(x), panel.getBottom(), panel.getBottom() + 4.f);
        const String label(sampleCount * numerator / 4);
        const Rectangle<float> labelBounds(x - 28.f, panel.getBottom() + 5.f, 56.f, 14.f);
        graphics.drawText(
                label,
                labelBounds,
                numerator == 0 ? Justification::centredLeft
                        : numerator == 4 ? Justification::centredRight
                                         : Justification::centred);
    }
}

void ImpulseResponseEditorComponent::layoutEditor() {
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(12, 12);
    impl->enabled.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::rowHeight),
            PropertyControlMetrics::labelWidth,
            PropertyControlMetrics::inlineGap);
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    for (auto* control : { &impl->size, &impl->postGain, &impl->highPass }) {
        control->setBounds(
                bounds.removeFromTop(PropertyControlMetrics::rowHeight),
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap,
                kValueWidth);
        bounds.removeFromTop(PropertyControlMetrics::rowGap);
    }
}

void ImpulseResponseEditorComponent::syncEditorFromNode() {
    ImpulseResponseNodeModel model;
    model.syncFromNode(node);
    impl->enabled.button.setToggleState(model.enabled, dontSendNotification);
    impl->size.slider.setValue(model.size, dontSendNotification);
    impl->postGain.slider.setValue(model.postGain, dontSendNotification);
    impl->highPass.slider.setValue(model.highPass, dontSendNotification);
    impl->size.refreshValueText();
    impl->postGain.refreshValueText();
    impl->highPass.refreshValueText();
}

void ImpulseResponseEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.button.getToggleState(),
            static_cast<float>(impl->size.slider.getValue()),
            static_cast<float>(impl->postGain.slider.getValue()),
            static_cast<float>(impl->highPass.slider.getValue()),
            0);
}

std::vector<NodeParameter> ImpulseResponseEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addEditorParameter(result, node, "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "size", "Size", String(impl->size.slider.getValue(), 8));
    addEditorParameter(result, node, "post", "Post Gain", String(impl->postGain.slider.getValue(), 8));
    addEditorParameter(result, node, "highPass", "High Pass", String(impl->highPass.slider.getValue(), 8));
    return result;
}

void ImpulseResponseEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("size", impl->size.slider.getValue());
    state.setProperty("postGain", impl->postGain.slider.getValue());
    state.setProperty("highPass", impl->highPass.slider.getValue());
    state.setProperty("sizeLayout", propertySliderRowAutomationState(impl->size));
    state.setProperty("postGainLayout", propertySliderRowAutomationState(impl->postGain));
    state.setProperty("highPassLayout", propertySliderRowAutomationState(impl->highPass));
    state.setProperty(
            "landmarks",
            sampleLandmarks(CycleDsp::irImpulseLength(impl->size.slider.getValue())));
    state.setProperty("resourceActionsAvailable", false);
}

}
