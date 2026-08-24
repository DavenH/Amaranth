#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"

#include "../../Graph/NodeParameterMap.h"

namespace CycleV2 {

namespace {

constexpr float kControlRailWidth = 236.f;
constexpr float kMaximumHostHeight = 560.f;

void showCompactValue(Slider& slider) {
    slider.setTextBoxStyle(Slider::TextBoxRight, false, 42, 22);
    slider.setNumDecimalPlacesToDisplay(2);
    slider.setColour(Slider::textBoxTextColourId, Colour(0xffaeb8c5));
    slider.setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour(Slider::textBoxHighlightColourId, Colour(0xff354659));
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

GuideCurveEditorComponent::GuideCurveEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    for (Slider* slider : {
            &impl->noise.slider,
            &impl->dcOffset.slider,
            &impl->phase.slider }) {
        slider->setRange(0.0, 1.0, 0.00001);
        showCompactValue(*slider);
    }
    bindDiscreteControl(impl->enabled);
    bindContinuousControls({ &impl->noise, &impl->dcOffset, &impl->phase });
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

Rectangle<float> GuideCurveEditorComponent::preferredHostBounds(Rectangle<float> canvasBounds) {
    Rectangle<float> available = canvasBounds.reduced(36.f, 24.f);
    return available.withHeight(jmin(available.getHeight(), kMaximumHostHeight))
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
    Rectangle<int> bounds = editorControlBounds().toNearestInt().reduced(16, 18);
    constexpr int labelWidth = 70;
    constexpr int labelGap = 10;
    constexpr int rowHeight = 30;
    constexpr int rowGap = 10;

    impl->enabled.setBounds(bounds.removeFromTop(rowHeight), labelWidth, labelGap);
    bounds.removeFromTop(rowGap);
    for (auto* slider : { &impl->noise, &impl->dcOffset, &impl->phase }) {
        slider->setBounds(bounds.removeFromTop(rowHeight), labelWidth, labelGap);
        bounds.removeFromTop(rowGap);
    }
}

void GuideCurveEditorComponent::syncEditorFromNode() {
    impl->enabled.button.setToggleState(guide.enabled, dontSendNotification);
    impl->noise.slider.setValue(guide.noise, dontSendNotification);
    impl->dcOffset.slider.setValue(guide.dcOffset, dontSendNotification);
    impl->phase.slider.setValue(guide.phase, dontSendNotification);
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
    result.push_back({ "noise", "Noise", String(impl->noise.slider.getValue()) });
    result.push_back({ "dcOffset", "DC Offset", String(impl->dcOffset.slider.getValue()) });
    result.push_back({ "phase", "Phase", String(impl->phase.slider.getValue()) });
    return result;
}

void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("noise", impl->noise.slider.getValue());
    state.setProperty("dcOffset", impl->dcOffset.slider.getValue());
    state.setProperty("phase", impl->phase.slider.getValue());
}

}
