#include "Nodes/Waveshaper/Editor/WaveshaperEditorComponent.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "UI/EffectEnableButton.h"
#include "UI/Editors/ProcessingScopeSelector.h"
#include "UI/Editors/PropertyControls.h"

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <cmath>
#include <cstdlib>

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 336.f;
constexpr float kPanelPreferredSize = 384.f;
constexpr int kOversamplingWidth = 72;
constexpr int kScopeWidth = 160;
constexpr int kScopeHeight = 28;

Rectangle<int> controlGroupBounds(Rectangle<float> controlArea) {
    Rectangle<int> available = controlArea.toNearestInt().reduced(12, 12);
    const int groupHeight = 3 * PropertyControlMetrics::groupLabelHeight
            + 2 * PropertyControlMetrics::compactRowHeight
            + 2 * PropertyControlMetrics::rowGap
            + 2 * PropertyControlMetrics::sectionGap
            + kScopeHeight
            + PropertyControlMetrics::rowHeight;
    return available.withSizeKeepingCentre(available.getWidth(), groupHeight);
}

String formatGain(double unitValue) {
    const float decibels = CycleDsp::waveshaperGainDecibels((float) unitValue);
    return (decibels > 0.f ? "+" : "") + formatPropertyReal(decibels) + " dB";
}

std::optional<double> parseGain(const String& text) {
    String number = text.trim();
    if (number.endsWithIgnoreCase("db")) {
        number = number.dropLastCharacters(2).trimEnd();
    }
    if (number.isEmpty()) {
        return std::nullopt;
    }

    const char* start = number.toRawUTF8();
    char* end {};
    const double decibels = std::strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(decibels)
            || decibels < -45.0 || decibels > 45.0) {
        return std::nullopt;
    }
    return CycleDsp::waveshaperGainUnitValue((float) decibels);
}

void configureGainControl(LabeledParameterSlider& control, const String& id) {
    control.slider.setRange(0.0, 1.0, 0.00001);
    control.slider.setComponentID("waveshaperEditor." + id);
    control.value.setComponentID("waveshaperEditor." + id + ".value");
    control.configureValuePresentation(
            formatGain,
            parseGain,
            0.5,
            1.0 / 90.0,
            0.1 / 90.0,
            "Gain in decibels. Shift-drag for fine adjustment; double-click for 0 dB.");
}

}

struct WaveshaperEditorComponent::Impl {
    explicit Impl(Component& owner) :
            processingScope ("waveshaperEditor")
        ,   preGain     (owner, "Pre Gain")
        ,   postGain    (owner, "Post Gain") {
        stylePropertyLabel(oversamplingLabel, "Antialiasing");
        owner.addAndMakeVisible(processingGroup);
        owner.addAndMakeVisible(processingScope);
        owner.addAndMakeVisible(gainGroup);
        owner.addAndMakeVisible(qualityGroup);
        owner.addAndMakeVisible(oversamplingLabel);
        owner.addAndMakeVisible(oversampling);
    }

    EffectEnableButton enabled;
    PropertyGroupLabel processingGroup { "PROCESSING" };
    ProcessingScopeSelector processingScope;
    PropertyGroupLabel gainGroup { "Gain" };
    PropertyGroupLabel qualityGroup { "Quality" };
    LabeledParameterSlider preGain;
    LabeledParameterSlider postGain;
    ComboBox oversampling;
    Label oversamplingLabel;
};

WaveshaperEditorComponent::WaveshaperEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    for (int value : { 1, 2, 4, 8 }) {
        impl->oversampling.addItem(String(value) + "x", value);
    }
    impl->oversampling.setComponentID("waveshaperEditor.oversampling");
    impl->oversampling.setTitle("Antialiasing");
    impl->oversampling.setDescription("Oversampling factor: 1x, 2x, 4x, or 8x");
    impl->oversampling.setTooltip("Oversampling factor: 1x, 2x, 4x, or 8x");
    impl->enabled.setComponentID("waveshaperEditor.enabled");
    setHeaderAction(impl->enabled);
    impl->processingScope.onChange = [this](const String& scope) {
        if (!setNodeParameterText("processingScope", "Processing", scope)) {
            syncEditorFromNode();
        }
    };

    configureGainControl(impl->preGain, "preGain");
    configureGainControl(impl->postGain, "postGain");
    impl->preGain.setCompactLayout(true);
    impl->postGain.setCompactLayout(true);

    bindDiscreteAction(impl->enabled, [] {});
    bindContinuousControls({ &impl->preGain, &impl->postGain });
    bindDiscreteControl(impl->oversampling);
}

WaveshaperEditorComponent::~WaveshaperEditorComponent() = default;

Rectangle<float> WaveshaperEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(kControlRailWidth);
}

Rectangle<float> WaveshaperEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(kControlRailWidth);
    bounds.reduce(12.f, 14.f);
    const float size = jmin(
            kPanelPreferredSize,
            jmin(bounds.getWidth(), bounds.getHeight()));
    return Rectangle<float>(size, size).withCentre({
            bounds.getX() + size * 0.5f,
            bounds.getCentreY() });
}

void WaveshaperEditorComponent::paintEditor(Graphics&) {
}

void WaveshaperEditorComponent::layoutEditor() {
    Rectangle<int> bounds = controlGroupBounds(editorControlBounds());
    impl->processingGroup.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::groupLabelHeight));
    impl->processingScope.setBounds(
            bounds.removeFromTop(kScopeHeight).withWidth(kScopeWidth));
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    impl->gainGroup.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::groupLabelHeight));
    impl->preGain.setBounds(bounds.removeFromTop(PropertyControlMetrics::compactRowHeight));
    bounds.removeFromTop(PropertyControlMetrics::rowGap);
    impl->postGain.setBounds(bounds.removeFromTop(PropertyControlMetrics::compactRowHeight));
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    impl->qualityGroup.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::groupLabelHeight));

    Rectangle<int> row = bounds.removeFromTop(PropertyControlMetrics::rowHeight);
    impl->oversamplingLabel.setBounds(row.removeFromLeft(PropertyControlMetrics::labelWidth));
    row.removeFromLeft(PropertyControlMetrics::inlineGap);
    impl->oversampling.setBounds(row.removeFromLeft(kOversamplingWidth));
}

void WaveshaperEditorComponent::syncEditorFromNode() {
    WaveshaperNodeModel model;
    model.syncFromNode(node);
    impl->enabled.setToggleState(model.enabled, dontSendNotification);
    impl->processingScope.setScope(
            NodeParameterMap(node).stringValue("processingScope", "voice"),
            dontSendNotification);
    impl->preGain.slider.setValue(model.preGain, dontSendNotification);
    impl->postGain.slider.setValue(model.postGain, dontSendNotification);
    impl->oversampling.setSelectedId(model.oversampling, dontSendNotification);
    impl->preGain.refreshValueText();
    impl->postGain.refreshValueText();
}

void WaveshaperEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.getToggleState(),
            static_cast<float>(impl->preGain.slider.getValue()),
            static_cast<float>(impl->postGain.slider.getValue()),
            0.5f,
            impl->oversampling.getSelectedId());
}

std::vector<NodeParameter> WaveshaperEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addEditorParameter(
            result,
            node,
            "processingScope",
            "Processing",
            impl->processingScope.scope());
    addEditorParameter(result, node, "enabled", "Enabled", impl->enabled.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "pre", "Pre Gain", String(impl->preGain.slider.getValue(), 8));
    addEditorParameter(result, node, "post", "Post Gain", String(impl->postGain.slider.getValue(), 8));
    addEditorParameter(result, node, "aaFactor", "AA Factor", String(impl->oversampling.getSelectedId()));
    return result;
}

void WaveshaperEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.getToggleState());
    state.setProperty("preGain", impl->preGain.slider.getValue());
    state.setProperty("postGain", impl->postGain.slider.getValue());
    state.setProperty("oversampling", impl->oversampling.getSelectedId());
    state.setProperty("oversamplingDisplay", impl->oversampling.getText());
    state.setProperty("processingScope", impl->processingScope.automationState());
    state.setProperty(
            "processingGroup",
            propertyGroupLabelAutomationState(impl->processingGroup));
    state.setProperty(
            "gainGroup",
            propertyGroupLabelAutomationState(impl->gainGroup));
    state.setProperty(
            "qualityGroup",
            propertyGroupLabelAutomationState(impl->qualityGroup));
    state.setProperty(
            "controlGroupBounds",
            editorBoundsToVar(controlGroupBounds(editorControlBounds()).toFloat()));
    state.setProperty("preGainLayout", propertySliderRowAutomationState(impl->preGain));
    state.setProperty("postGainLayout", propertySliderRowAutomationState(impl->postGain));
}

}
