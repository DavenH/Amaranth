#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"

#include "../../Graph/NodeParameterMap.h"

namespace CycleV2 {

struct GuideCurveEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enable")
        ,   noise       (owner, "Noise")
        ,   dcOffset    (owner, "DC Offset")
        ,   phase       (owner, "Phase") {
        owner.addAndMakeVisible(add);
        owner.addAndMakeVisible(remove);
        styleParameterButton(add, "+");
        styleParameterButton(remove, "-");
    }

    ParameterToggle enabled;
    LabeledParameterSlider noise;
    LabeledParameterSlider dcOffset;
    LabeledParameterSlider phase;
    TextButton add;
    TextButton remove;
};

GuideCurveEditorComponent::GuideCurveEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    for (Slider* slider : {
            &impl->noise.slider,
            &impl->dcOffset.slider,
            &impl->phase.slider }) {
        slider->setRange(0.0, 1.0, 0.00001);
    }
    bindDiscreteControl(impl->enabled);
    bindContinuousControls({ &impl->noise, &impl->dcOffset, &impl->phase });
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

void GuideCurveEditorComponent::setGuideResource(const GuideCurveResource& nextGuide) {
    guide = nextGuide;
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
    return bounds.removeFromRight(196.f);
}

Rectangle<float> GuideCurveEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(196.f);
    return bounds;
}

void GuideCurveEditorComponent::paintEditor(Graphics&) {
}

void GuideCurveEditorComponent::layoutEditor() {
    ParameterRail::layout(
            editorControlBounds(),
            impl->enabled,
            { &impl->noise, &impl->dcOffset, &impl->phase },
            { &impl->add, &impl->remove });
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
