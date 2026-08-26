#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

#include "Graph/NodeParameterMap.h"

#include <cmath>
#include <cstdlib>

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 336.f;
constexpr float kMaximumHostWidth = 1100.f;
constexpr float kMaximumHostHeight = 560.f;

String formatPercentage(double value) {
    return String(value * 100.0, 1) + "%";
}

std::optional<double> parsePercentage(const String& text) {
    String number = text.trim();
    if (number.endsWithChar('%')) {
        number = number.dropLastCharacters(1).trimEnd();
    }
    if (number.isEmpty()) {
        return std::nullopt;
    }

    const char* start = number.toRawUTF8();
    char* end {};
    const double parsed = std::strtod(start, &end);
    if (end == start || *end != '\0' || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed / 100.0;
}

}

struct GuideCurveEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enabled")
        ,   noise       (owner, "Noise")
        ,   dcOffset    (owner, "DC Offset")
        ,   phase       (owner, "Phase") {}

    ParameterToggle enabled;
    LabeledParameterSlider noise;
    LabeledParameterSlider dcOffset;
    LabeledParameterSlider phase;
};

GuideCurveEditorComponent::GuideCurveEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    struct ControlSetup {
        LabeledParameterSlider* control;
        const char* id;
        const char* help;
    };
    for (const auto& setup : {
            ControlSetup {
                    &impl->noise,
                    "noise",
                    "Random noise depth. Shift-drag for fine adjustment; double-click to reset."
            },
            ControlSetup {
                    &impl->dcOffset,
                    "dcOffset",
                    "Random DC offset range. Shift-drag for fine adjustment; double-click to reset."
            },
            ControlSetup {
                    &impl->phase,
                    "phase",
                    "Random phase range. Shift-drag for fine adjustment; double-click to reset."
            } }) {
        setup.control->slider.setRange(0.0, 1.0, 0.00001);
        setup.control->slider.setComponentID("guideEditor." + String(setup.id));
        setup.control->value.setComponentID("guideEditor." + String(setup.id) + ".value");
        setup.control->configureValuePresentation(
                formatPercentage,
                parsePercentage,
                0.0,
                0.01,
                0.001,
                setup.help);
    }
    bindDiscreteControl(impl->enabled);
    bindContinuousControls({ &impl->noise, &impl->dcOffset, &impl->phase });
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

Rectangle<float> GuideCurveEditorComponent::preferredHostBounds(Rectangle<float> canvasBounds) {
    Rectangle<float> available = canvasBounds.reduced(36.f, 24.f);
    return available.withSizeKeepingCentre(
                    jmin(available.getWidth(), kMaximumHostWidth),
                    jmin(available.getHeight(), kMaximumHostHeight))
            .withCentre(available.getCentre());
}

void GuideCurveEditorComponent::setGuideResource(const GuideCurveResource& nextGuide) {
    guide = nextGuide;
    setEditorModelState(guide.model);
    widget.syncFromGuideResource(guide);
    const ScopedValueSetter<bool> guard(syncingControls, true);
    syncEditorFromNode();
    applyEditorStateToWidget();
    refreshEditorSubject();
}

void GuideCurveEditorComponent::renderOpenGL(float scaleFactor) {
    widget.renderGuideExpandedPanelOpenGL(
            editorPanelBounds().translated((float) getX(), (float) getY()),
            getLocalBounds().toFloat().translated((float) getX(), (float) getY()),
            scaleFactor);
}

std::vector<std::pair<String, Rectangle<float>>> GuideCurveEditorComponent::automationPointerTargets() const {
    const std::pair<const char*, const char*> targetIds[] {
            { "guideEditor.noise", "guideEditor.noise" },
            { "guideEditor.noise.value", "guideEditor.noise.value" },
            { "guideEditor.dcOffset", "guideEditor.dcOffset" },
            { "guideEditor.dcOffset.value", "guideEditor.dcOffset.value" },
            { "guideEditor.phase", "guideEditor.phase" },
            { "guideEditor.phase.value", "guideEditor.phase.value" },
            { "guideEditor.close", "curveEditor.close" }
    };
    std::vector<std::pair<String, Rectangle<float>>> result;
    for (const auto& [semanticId, componentId] : targetIds) {
        const Component* target = findChildWithID(componentId);
        if (target != nullptr) {
            result.emplace_back(
                    semanticId,
                    getLocalArea(target, target->getLocalBounds()).toFloat());
        }
    }
    return result;
}

Rectangle<float> GuideCurveEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(kControlRailWidth);
}

Rectangle<float> GuideCurveEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(kControlRailWidth);
    return bounds;
}

void GuideCurveEditorComponent::paintEditor(Graphics&) {
}

void GuideCurveEditorComponent::layoutEditor() {
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(12, 12);
    impl->enabled.setBounds(
            bounds.removeFromTop(PropertyControlMetrics::rowHeight),
            PropertyControlMetrics::labelWidth,
            PropertyControlMetrics::inlineGap);
    bounds.removeFromTop(PropertyControlMetrics::sectionGap);
    for (auto* slider : { &impl->noise, &impl->dcOffset, &impl->phase }) {
        slider->setBounds(
                bounds.removeFromTop(PropertyControlMetrics::rowHeight),
                PropertyControlMetrics::labelWidth,
                PropertyControlMetrics::inlineGap);
        bounds.removeFromTop(PropertyControlMetrics::rowGap);
    }
}

void GuideCurveEditorComponent::syncEditorFromNode() {
    impl->enabled.button.setToggleState(guide.enabled, dontSendNotification);
    impl->noise.slider.setValue(guide.noise, dontSendNotification);
    impl->dcOffset.slider.setValue(guide.dcOffset, dontSendNotification);
    impl->phase.slider.setValue(guide.phase, dontSendNotification);
    impl->noise.refreshValueText();
    impl->dcOffset.refreshValueText();
    impl->phase.refreshValueText();
}

void GuideCurveEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.button.getToggleState(),
            static_cast<float>(impl->noise.slider.getValue()),
            static_cast<float>(impl->dcOffset.slider.getValue()),
            static_cast<float>(impl->phase.slider.getValue()),
            0);
}

std::vector<NodeParameter> GuideCurveEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    result.push_back({ "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0" });
    result.push_back({ "noise", "Noise", String(impl->noise.slider.getValue(), 8) });
    result.push_back({ "dcOffset", "DC Offset", String(impl->dcOffset.slider.getValue(), 8) });
    result.push_back({ "phase", "Phase", String(impl->phase.slider.getValue(), 8) });
    return result;
}

void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("noise", impl->noise.slider.getValue());
    state.setProperty("dcOffset", impl->dcOffset.slider.getValue());
    state.setProperty("phase", impl->phase.slider.getValue());
    state.setProperty("noiseLayout", propertySliderRowAutomationState(impl->noise));
    state.setProperty("dcOffsetLayout", propertySliderRowAutomationState(impl->dcOffset));
    state.setProperty("phaseLayout", propertySliderRowAutomationState(impl->phase));
}

}
