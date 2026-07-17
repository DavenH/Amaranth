#include "CurveNodeEditors.h"

#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"
#include "EnvelopeMorphControls.h"
#include "../Trimesh/TrimeshSidePanelRenderer.h"

#include <array>

namespace CycleV2 {

struct EnvelopeEditorComponent::Impl {
    explicit Impl(Component& owner) :
            redMorph    (owner, "Red")
        ,   blueMorph   (owner, "Blue") {
        styleParameterLabel(timeLabel, "Time");
        owner.addAndMakeVisible(timeLabel);
        for (auto* button : { &loop, &sustain, &logarithmic, &dynamic }) {
            styleParameterButton(*button, button->getButtonText());
            owner.addAndMakeVisible(*button);
        }
        logarithmic.setClickingTogglesState(true);
        dynamic.setClickingTogglesState(true);
    }

    LabeledParameterSlider redMorph;
    LabeledParameterSlider blueMorph;
    Label timeLabel;
    TextButton loop { "Loop" };
    TextButton sustain { "Sustain" };
    TextButton logarithmic { "Log" };
    TextButton dynamic { "Live" };
    EnvelopeMorphControls presentation;

    int viewAxis {};
    bool redLinked { true };
    bool blueLinked { true };
    bool draggingMorph {};
    bool draggingParameter {};
    String parameterId;
};

EnvelopeEditorComponent::EnvelopeEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    bindContinuousControl(impl->redMorph);
    bindContinuousControl(impl->blueMorph);

    bindDiscreteAction(impl->loop, [this] {
        widget.toggleSelectedEnvelopeMarker(true);
    });
    bindDiscreteAction(impl->sustain, [this] {
        widget.toggleSelectedEnvelopeMarker(false);
    });
    bindDiscreteAction(impl->logarithmic, [this] {
        widget.setEnvelopeLogarithmic(impl->logarithmic.getToggleState());
    });
    bindDiscreteAction(impl->dynamic, [] {});
}

EnvelopeEditorComponent::~EnvelopeEditorComponent() = default;

Rectangle<float> EnvelopeEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromTop(EnvelopeMorphControls::controlsHeight);
}

Rectangle<float> EnvelopeEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromTop(EnvelopeMorphControls::controlsHeight);
    return bounds;
}

void EnvelopeEditorComponent::paintEditor(Graphics& graphics) {
    const auto controls = editorControlBounds();
    impl->presentation.draw(
            graphics,
            controls,
            static_cast<float>(impl->redMorph.slider.getValue()),
            static_cast<float>(impl->blueMorph.slider.getValue()),
            impl->viewAxis,
            impl->redLinked,
            impl->blueLinked);

    std::array<String, 6> guides {};
    TrimeshSidePanelRenderer::drawVertexParameters(
            graphics,
            impl->presentation.vertexBounds(controls),
            widget.selectedVertexParameters(),
            guides);
}

void EnvelopeEditorComponent::layoutEditor() {
    const auto controls = editorControlBounds();
    auto timeRow = impl->presentation.morphRow(controls, 0).toNearestInt();
    impl->timeLabel.setBounds(timeRow.removeFromLeft(42));

    auto redRow = impl->presentation.morphRow(controls, 1).toNearestInt();
    redRow.removeFromRight(58);
    impl->redMorph.setBounds(redRow, 42, 0);

    auto blueRow = impl->presentation.morphRow(controls, 2).toNearestInt();
    blueRow.removeFromRight(58);
    impl->blueMorph.setBounds(blueRow, 42, 0);

    auto markerRow = impl->presentation.railColumn(controls).toNearestInt();
    markerRow.removeFromTop(160);
    markerRow = markerRow.removeFromTop(30);
    impl->loop.setBounds(markerRow.removeFromLeft(70).reduced(2));
    markerRow.removeFromLeft(8);
    impl->sustain.setBounds(markerRow.removeFromLeft(70).reduced(2));
    markerRow.removeFromLeft(8);
    impl->logarithmic.setBounds(markerRow.removeFromLeft(54).reduced(2));
    markerRow.removeFromLeft(8);
    impl->dynamic.setBounds(markerRow.removeFromLeft(54).reduced(2));
}

void EnvelopeEditorComponent::syncEditorFromNode() {
    EnvelopeNodeModel model;
    model.syncFromNode(node);
    impl->redMorph.slider.setValue(model.red, dontSendNotification);
    impl->blueMorph.slider.setValue(model.blue, dontSendNotification);
    impl->redLinked = model.redLinked;
    impl->blueLinked = model.blueLinked;
    impl->logarithmic.setToggleState(model.logarithmic, dontSendNotification);
    impl->dynamic.setToggleState(model.dynamicWhileLive, dontSendNotification);
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
    widget.setEnvelopeLogarithmic(model.logarithmic);
    impl->loop.setToggleState(widget.selectedEnvelopeMarkerState(true), dontSendNotification);
    impl->sustain.setToggleState(widget.selectedEnvelopeMarkerState(false), dontSendNotification);
}

void EnvelopeEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            true,
            static_cast<float>(impl->redMorph.slider.getValue()),
            static_cast<float>(impl->blueMorph.slider.getValue()),
            0.5f,
            0);
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
    widget.setEnvelopeLogarithmic(impl->logarithmic.getToggleState());
}

std::vector<NodeParameter> EnvelopeEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    addEditorParameter(result, node, "logarithmic", "Logarithmic", impl->logarithmic.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "dynamic", "Dynamic While Live", impl->dynamic.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "red", "Red Morph", String(impl->redMorph.slider.getValue()));
    addEditorParameter(result, node, "blue", "Blue Morph", String(impl->blueMorph.slider.getValue()));
    addEditorParameter(result, node, "level", "Level", retainedEditorParameter(node, "level", "1"));
    return result;
}

void EnvelopeEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    state.setProperty("redMorph", impl->redMorph.slider.getValue());
    state.setProperty("blueMorph", impl->blueMorph.slider.getValue());
    state.setProperty("viewAxis", impl->viewAxis);
    state.setProperty("logarithmic", impl->logarithmic.getToggleState());
    state.setProperty("dynamic", impl->dynamic.getToggleState());
    state.setProperty(
            "morphPlaneBounds",
            editorBoundsToVar(impl->presentation.planeBounds(editorControlBounds())));
}

bool EnvelopeEditorComponent::editorMouseMove(Point<float> position) {
    const auto controls = editorControlBounds();
    bool interactive = impl->presentation.planeBounds(controls).contains(position);
    for (int axis = 0; axis < 3; ++axis) {
        interactive = interactive || impl->presentation.axisBounds(controls, axis).contains(position);
        interactive = interactive
                || (axis > 0 && impl->presentation.linkBounds(controls, axis).contains(position));
    }

    const auto parameters = widget.selectedVertexParameters();
    const auto parameterArea = impl->presentation.vertexBounds(controls);
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, index);
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        const auto guide = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);
        interactive = interactive || rail.expanded(5.f, 8.f).contains(position);
        interactive = interactive || guide.expanded(4.f).contains(position);
    }
    setMouseCursor(interactive ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    return interactive;
}

bool EnvelopeEditorComponent::editorMouseDown(Point<float> position) {
    impl->draggingMorph = false;
    impl->draggingParameter = false;
    impl->parameterId.clear();
    const auto controls = editorControlBounds();

    if (handleAxisMouseDown(position, controls)) {
        return true;
    }
    if (handleVertexParameterMouseDown(position, controls)) {
        return true;
    }
    const auto plane = impl->presentation.planeBounds(controls);
    if (plane.contains(position)) {
        impl->draggingMorph = true;
        return editorMouseDrag(position);
    }
    return false;
}

bool EnvelopeEditorComponent::handleAxisMouseDown(
        Point<float> position,
        Rectangle<float> controls) {
    for (int axis = 0; axis < 3; ++axis) {
        if (impl->presentation.axisBounds(controls, axis).contains(position)) {
            impl->viewAxis = axis;
            requestRepaint();
            return true;
        }
        if (axis > 0 && impl->presentation.linkBounds(controls, axis).contains(position)) {
            bool& linked = axis == 1 ? impl->redLinked : impl->blueLinked;
            linked = !linked;
            publishCurrentState();
            requestRepaint();
            return true;
        }
    }
    return false;
}

bool EnvelopeEditorComponent::handleVertexParameterMouseDown(
        Point<float> position,
        Rectangle<float> controls) {
    const auto parameters = widget.selectedVertexParameters();
    const auto parameterArea = impl->presentation.vertexBounds(controls);
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, index);
        const auto guide = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);
        if (guide.expanded(4.f).contains(position)) {
            PopupMenu menu;
            menu.addItem(1, "Guide attachments are available on mesh nodes", false, false);
            const auto target = localAreaToGlobal(guide.toNearestInt());
            menu.showMenuAsync(PopupMenu::Options().withTargetScreenArea(target));
            return true;
        }

        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        if (rail.expanded(5.f, 8.f).contains(position)) {
            impl->parameterId = parameters[static_cast<size_t>(index)].id;
            impl->draggingParameter = true;
            const float value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
            widget.setSelectedVertexParameter(impl->parameterId, value);
            return true;
        }
    }
    return false;
}

bool EnvelopeEditorComponent::editorMouseDrag(Point<float> position) {
    if (impl->draggingMorph) {
        return dragMorph(position);
    }
    if (!impl->draggingParameter) {
        return false;
    }
    return dragVertexParameter(position);
}

bool EnvelopeEditorComponent::dragMorph(Point<float> position) {
    const auto plane = impl->presentation.planeBounds(editorControlBounds());
    const float red = jlimit(0.f, 1.f, (position.x - plane.getX()) / plane.getWidth());
    const float blue = jlimit(0.f, 1.f, (plane.getBottom() - position.y) / plane.getHeight());
    impl->redMorph.slider.setValue(red, sendNotificationSync);
    impl->blueMorph.slider.setValue(blue, sendNotificationSync);
    return true;
}

bool EnvelopeEditorComponent::dragVertexParameter(Point<float> position) {
    const auto parameters = widget.selectedVertexParameters();
    const auto area = impl->presentation.vertexBounds(editorControlBounds());
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        if (parameters[static_cast<size_t>(index)].id != impl->parameterId) {
            continue;
        }
        const auto row = TrimeshSidePanelRenderer::vertexParameterRowBounds(area, index);
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        const float value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        if (widget.setSelectedVertexParameter(impl->parameterId, value)) {
            requestRepaint();
        }
        return true;
    }
    return false;
}

void EnvelopeEditorComponent::editorMouseUp() {
    impl->draggingMorph = false;
    impl->draggingParameter = false;
    impl->parameterId.clear();
}

}
