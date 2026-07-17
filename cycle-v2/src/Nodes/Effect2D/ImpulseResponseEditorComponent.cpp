#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"

namespace CycleV2 {

struct ImpulseResponseEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     (owner, "Enable")
        ,   size        (owner, "Size")
        ,   postGain    (owner, "Post")
        ,   highPass    (owner, "HighPass") {
        for (auto* button : { &load, &unload, &model }) {
            owner.addAndMakeVisible(*button);
        }
        styleParameterButton(load, "Load");
        styleParameterButton(unload, "Unload");
        styleParameterButton(model, "Model");
    }

    ParameterToggle enabled;
    LabeledParameterSlider size;
    LabeledParameterSlider postGain;
    LabeledParameterSlider highPass;
    TextButton load;
    TextButton unload;
    TextButton model;
};

ImpulseResponseEditorComponent::ImpulseResponseEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    bindDiscreteControl(impl->enabled);
    bindContinuousControl(impl->size);
    bindContinuousControl(impl->postGain);
    bindContinuousControl(impl->highPass);
}

ImpulseResponseEditorComponent::~ImpulseResponseEditorComponent() = default;

Rectangle<float> ImpulseResponseEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(212.f);
}

Rectangle<float> ImpulseResponseEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(212.f);
    return bounds;
}

void ImpulseResponseEditorComponent::paintEditor(Graphics&) {}

void ImpulseResponseEditorComponent::layoutEditor() {
    ParameterRail::layout(
            editorControlBounds(),
            impl->enabled,
            { &impl->size, &impl->postGain, &impl->highPass },
            { &impl->load, &impl->unload, &impl->model });
}

void ImpulseResponseEditorComponent::syncEditorFromNode() {
    ImpulseResponseNodeModel model;
    model.syncFromNode(node);
    impl->enabled.button.setToggleState(model.enabled, dontSendNotification);
    impl->size.slider.setValue(model.size, dontSendNotification);
    impl->postGain.slider.setValue(model.postGain, dontSendNotification);
    impl->highPass.slider.setValue(model.highPass, dontSendNotification);
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
    addEditorParameter(result, node, "size", "Size", String(impl->size.slider.getValue()));
    addEditorParameter(result, node, "post", "Post Gain", String(impl->postGain.slider.getValue()));
    addEditorParameter(result, node, "highPass", "High Pass", String(impl->highPass.slider.getValue()));
    return result;
}

void ImpulseResponseEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("size", impl->size.slider.getValue());
    state.setProperty("postGain", impl->postGain.slider.getValue());
    state.setProperty("highPass", impl->highPass.slider.getValue());
}

}
