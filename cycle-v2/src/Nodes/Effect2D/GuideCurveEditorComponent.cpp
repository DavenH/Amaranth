#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"

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
    const auto publish = [this] {
        publishCurrentState();
        requestRepaint();
    };
    const auto begin = [this] {
        beginTransaction();
    };
    const auto commit = [this] {
        commitTransaction();
    };
    impl->enabled.bind(publish);
    impl->noise.bind(publish, begin, commit);
    impl->dcOffset.bind(publish, begin, commit);
    impl->phase.bind(publish, begin, commit);
}

GuideCurveEditorComponent::~GuideCurveEditorComponent() = default;

Rectangle<float> GuideCurveEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromRight(196.f);
}

Rectangle<float> GuideCurveEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromRight(196.f);
    return bounds;
}

void GuideCurveEditorComponent::paintEditor(Graphics&) {}

void GuideCurveEditorComponent::layoutEditor() {
    ParameterRail::layout(
            editorControlBounds(),
            impl->enabled,
            { &impl->noise, &impl->dcOffset, &impl->phase },
            { &impl->add, &impl->remove });
}

void GuideCurveEditorComponent::syncEditorFromNode() {
    GuideCurveNodeModel model;
    model.syncFromNode(node);
    impl->enabled.button.setToggleState(model.enabled, dontSendNotification);
    impl->noise.slider.setValue(model.noise, dontSendNotification);
    impl->dcOffset.slider.setValue(model.dcOffset, dontSendNotification);
    impl->phase.slider.setValue(model.phase, dontSendNotification);
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
    addEditorParameter(result, node, "enabled", "Enabled", impl->enabled.button.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "noise", "Noise", String(impl->noise.slider.getValue()));
    addEditorParameter(result, node, "dcOffset", "DC Offset", String(impl->dcOffset.slider.getValue()));
    addEditorParameter(result, node, "phase", "Phase", String(impl->phase.slider.getValue()));
    return result;
}

void GuideCurveEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("enabled", impl->enabled.button.getToggleState());
    state.setProperty("noise", impl->noise.slider.getValue());
    state.setProperty("dcOffset", impl->dcOffset.slider.getValue());
    state.setProperty("phase", impl->phase.slider.getValue());
}

}
