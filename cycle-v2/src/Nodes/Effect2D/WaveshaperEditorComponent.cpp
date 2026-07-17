#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"

namespace CycleV2 {

struct WaveshaperEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enable")
        ,   preGain     (owner, "Pre")
        ,   postGain    (owner, "Post") {
        styleParameterLabel(oversamplingLabel, "AA factor");
        owner.addAndMakeVisible(oversamplingLabel);
        owner.addAndMakeVisible(oversampling);
    }

    ParameterToggle enabled;
    LabeledParameterSlider preGain;
    LabeledParameterSlider postGain;
    ComboBox oversampling;
    Label oversamplingLabel;
};

WaveshaperEditorComponent::WaveshaperEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    for (int value : { 1, 2, 4, 8 }) {
        impl->oversampling.addItem(String(value), value);
    }

    bindDiscreteControl(impl->enabled);
    bindContinuousControl(impl->preGain);
    bindContinuousControl(impl->postGain);
    bindDiscreteControl(impl->oversampling);
}

WaveshaperEditorComponent::~WaveshaperEditorComponent() = default;

Rectangle<float> WaveshaperEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(224.f);
}

Rectangle<float> WaveshaperEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(224.f);
    bounds.reduce(18.f, 14.f);
    const float size = jmin(300.f, jmin(bounds.getWidth(), bounds.getHeight()));
    return Rectangle<float>(size, size).withCentre({ bounds.getX() + size * 0.5f, bounds.getCentreY() });
}

void WaveshaperEditorComponent::paintEditor(Graphics&) {}

void WaveshaperEditorComponent::layoutEditor() {
    auto bounds = editorControlBounds().toNearestInt().reduced(12, 8);
    bounds = bounds.withSizeKeepingCentre(
            jmin(bounds.getWidth(), 206),
            jmin(bounds.getHeight(), 175));

    impl->enabled.setBounds(bounds.removeFromTop(34), 78, 12);
    bounds.removeFromTop(13);
    impl->preGain.setBounds(bounds.removeFromTop(34), 78, 12);
    bounds.removeFromTop(13);
    impl->postGain.setBounds(bounds.removeFromTop(34), 78, 12);
    bounds.removeFromTop(13);

    auto row = bounds.removeFromTop(34);
    impl->oversamplingLabel.setBounds(row.removeFromLeft(78));
    row.removeFromLeft(12);
    impl->oversampling.setBounds(row.removeFromLeft(116).withSizeKeepingCentre(116, 32));
}

void WaveshaperEditorComponent::syncEditorFromNode() {
    WaveshaperNodeModel model;
    model.syncFromNode(node);
    impl->enabled.button.setToggleState(model.enabled, dontSendNotification);
    impl->preGain.slider.setValue(model.preGain, dontSendNotification);
    impl->postGain.slider.setValue(model.postGain, dontSendNotification);
    impl->oversampling.setSelectedId(model.oversampling, dontSendNotification);
}

void WaveshaperEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            impl->enabled.button.getToggleState(),
            static_cast<float>(impl->preGain.slider.getValue()),
            static_cast<float>(impl->postGain.slider.getValue()),
            0.5f,
            impl->oversampling.getSelectedId());
}

std::vector<NodeParameter> WaveshaperEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addEditorParameter(result, node, "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "pre", "Pre Gain", String(impl->preGain.slider.getValue()));
    addEditorParameter(result, node, "post", "Post Gain", String(impl->postGain.slider.getValue()));
    addEditorParameter(result, node, "aaFactor", "AA Factor", String(impl->oversampling.getSelectedId()));
    return result;
}

void WaveshaperEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("preGain", impl->preGain.slider.getValue());
    state.setProperty("postGain", impl->postGain.slider.getValue());
    state.setProperty("oversampling", impl->oversampling.getSelectedId());
}

}
