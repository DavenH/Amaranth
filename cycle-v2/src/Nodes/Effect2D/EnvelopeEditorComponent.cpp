#include <Binary/Images.h>

#include <array>

#include "CurveNodeEditors.h"
#include "CurveEditorPrimitives.h"
#include "CurveNodeModels.h"
#include "EnvelopeMorphControls.h"
#include "../Envelope/EnvelopePurpose.h"
#include "../Trimesh/TrimeshSidePanelRenderer.h"
#include "../../UI/EnvelopePurposeSelector.h"

namespace CycleV2 {

namespace {

Image cycleV1EnvelopeIcon(int atlasX, int atlasY) {
    static const Image atlas = PNGImageFormat::loadFrom(
            Images::icons_png, Images::icons_pngSize);
    return atlas.getClippedImage({ atlasX * 24, atlasY * 24, 24, 24 });
}

void styleEnvelopeRangeButton(
        ImageButton& button,
        int atlasY,
        const String& tooltip) {
    const Image icon = cycleV1EnvelopeIcon(6, atlasY);
    button.setImages(
            false,
            false,
            true,
            icon,
            0.74f,
            Colours::transparentBlack,
            icon,
            1.f,
            Colours::transparentBlack,
            icon,
            0.86f,
            Colours::black.withAlpha(0.18f));
    button.setTooltip(tooltip);
    button.setMouseCursor(MouseCursor::PointingHandCursor);
    button.setWantsKeyboardFocus(false);
}

}

struct EnvelopeEditorComponent::Impl {
    explicit Impl(Component& owner) :
            redMorph    (owner, "Red")
        ,   blueMorph   (owner, "Blue") {
        styleParameterLabel(timeLabel, "Time");
        styleParameterLabel(modeLabel, "Mode");
        owner.addAndMakeVisible(timeLabel);
        owner.addAndMakeVisible(modeLabel);
        owner.addAndMakeVisible(mode);
        for (auto* button : { &loop, &sustain, &logarithmic }) {
            styleParameterButton(*button, button->getButtonText());
            owner.addAndMakeVisible(*button);
        }
        styleEnvelopeRangeButton(fitVertical, 0, "Fit envelope vertical range");
        styleEnvelopeRangeButton(fullVertical, 1, "Show full envelope vertical range");
        owner.addAndMakeVisible(fitVertical);
        owner.addAndMakeVisible(fullVertical);
        logarithmic.setClickingTogglesState(true);
    }

    LabeledParameterSlider redMorph;
    LabeledParameterSlider blueMorph;
    Label timeLabel;
    Label modeLabel;
    EnvelopePurposeSelector mode;
    TextButton loop { "Loop" };
    TextButton sustain { "Sustain" };
    TextButton logarithmic { "Log" };
    ImageButton fitVertical { "Fit envelope vertical range" };
    ImageButton fullVertical { "Show full envelope vertical range" };
    EnvelopeMorphControls presentation;

    EnvelopePurpose appliedPurpose { EnvelopePurpose::Control };
    int viewAxis {};
    bool hasAppliedPurpose {};
    bool redLinked { true };
    bool blueLinked { true };
    bool draggingMorph {};
    bool draggingParameter {};
    String parameterId;
};

EnvelopeEditorComponent::EnvelopeEditorComponent(Effect2DWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    bindContinuousControls({ &impl->redMorph, &impl->blueMorph });
    impl->mode.onChange = [this](EnvelopePurpose) {
        publishDiscreteControlChange();
    };

    bindDiscreteAction(impl->loop, [this] {
        widget.toggleSelectedEnvelopeMarker(true);
    });
    bindDiscreteAction(impl->sustain, [this] {
        widget.toggleSelectedEnvelopeMarker(false);
    });
    bindDiscreteAction(impl->logarithmic, [this] {
        widget.setEnvelopeLogarithmic(impl->logarithmic.getToggleState());
    });
    impl->fitVertical.onClick = [this] {
        widget.fitEnvelopeVerticalRange();
        requestRepaint();
    };
    impl->fullVertical.onClick = [this] {
        widget.resetEnvelopeVerticalRange();
        requestRepaint();
    };
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
            guides,
            EnvelopeMorphControls::vertexParameterHeightScale);

    if (impl->mode.purpose() == EnvelopePurpose::Pitch) {
        auto pitchLabels = editorPanelBounds().removeFromRight(48.f);
        graphics.setColour(Colours::white.withAlpha(0.72f));
        graphics.setFont(11.f);
        graphics.drawText("+ pitch", pitchLabels.removeFromTop(18.f), Justification::centredRight);
        const auto neutral = pitchLabels.withY(pitchLabels.getCentreY() - 9.f).withHeight(18.f);
        graphics.drawText("neutral", neutral, Justification::centredRight);
        graphics.drawText(String::fromUTF8("− pitch"), pitchLabels.removeFromBottom(18.f), Justification::centredRight);
    }
}

void EnvelopeEditorComponent::layoutEditor() {
    const auto controls = editorControlBounds();
    auto modeRow = impl->presentation.modeRow(controls).toNearestInt();
    impl->modeLabel.setBounds(modeRow.removeFromLeft(56));
    impl->mode.setBounds(modeRow.removeFromLeft(152).reduced(2));

    auto timeRow = impl->presentation.morphRow(controls, 0).toNearestInt();
    impl->timeLabel.setBounds(timeRow.removeFromLeft(42));

    auto redRow = impl->presentation.morphRow(controls, 1).toNearestInt();
    redRow.removeFromRight(58);
    impl->redMorph.setBounds(redRow, 42, 0);

    auto blueRow = impl->presentation.morphRow(controls, 2).toNearestInt();
    blueRow.removeFromRight(58);
    impl->blueMorph.setBounds(blueRow, 42, 0);

    auto markerRow = impl->presentation.actionRow(controls).toNearestInt();
    auto rangeActions = markerRow.removeFromRight(56);
    impl->fitVertical.setBounds(rangeActions.removeFromLeft(24).withSizeKeepingCentre(24, 24));
    rangeActions.removeFromLeft(8);
    impl->fullVertical.setBounds(rangeActions.removeFromLeft(24).withSizeKeepingCentre(24, 24));
    markerRow.removeFromLeft(59);
    impl->loop.setBounds(markerRow.removeFromLeft(70).reduced(2));
    markerRow.removeFromLeft(8);
    impl->sustain.setBounds(markerRow.removeFromLeft(70).reduced(2));
    markerRow.removeFromLeft(8);
    impl->logarithmic.setBounds(markerRow.removeFromLeft(54).reduced(2));
}

void EnvelopeEditorComponent::syncEditorFromNode() {
    EnvelopeNodeModel model;
    model.syncFromNode(node);
    const EnvelopePurpose purpose = envelopePurposeFor(node);
    impl->mode.setPurpose(purpose);
    impl->redMorph.slider.setValue(model.red, dontSendNotification);
    impl->blueMorph.slider.setValue(model.blue, dontSendNotification);
    impl->redLinked = model.redLinked;
    impl->blueLinked = model.blueLinked;
    impl->logarithmic.setToggleState(model.logarithmic, dontSendNotification);
    impl->logarithmic.setEnabled(envelopePurposeAllowsLogarithmic(purpose));
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
    const EnvelopePurpose purpose = impl->mode.purpose();
    const bool enteringPitch = purpose == EnvelopePurpose::Pitch
            && (!impl->hasAppliedPurpose || impl->appliedPurpose != EnvelopePurpose::Pitch);
    widget.setEnvelopeBipolar(purpose == EnvelopePurpose::Pitch);
    widget.setEnvelopeLogarithmic(
            envelopePurposeAllowsLogarithmic(purpose)
                    && impl->logarithmic.getToggleState());
    impl->logarithmic.setEnabled(envelopePurposeAllowsLogarithmic(purpose));
    if (enteringPitch) {
        widget.fitEnvelopeVerticalRange();
    }
    impl->appliedPurpose = purpose;
    impl->hasAppliedPurpose = true;
}

std::vector<NodeParameter> EnvelopeEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    const EnvelopePurpose purpose = impl->mode.purpose();
    addEditorParameter(
            result,
            node,
            "purpose",
            "Purpose",
            envelopePurposeToString(purpose));
    addEditorParameter(
            result,
            node,
            "logarithmic",
            "Logarithmic",
            envelopePurposeAllowsLogarithmic(purpose)
                    && impl->logarithmic.getToggleState() ? "1" : "0");
    addEditorParameter(result, node, "red", "Red Morph", String(impl->redMorph.slider.getValue()));
    addEditorParameter(result, node, "blue", "Blue Morph", String(impl->blueMorph.slider.getValue()));
    addEditorParameter(result, node, "level", "Level", retainedEditorParameter(node, "level", "1"));
    return result;
}

void EnvelopeEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    const auto controls = editorControlBounds();
    state.setProperty("redMorph", impl->redMorph.slider.getValue());
    state.setProperty("blueMorph", impl->blueMorph.slider.getValue());
    state.setProperty("viewAxis", impl->viewAxis);
    state.setProperty("modeLabel", "Mode");
    state.setProperty("mode", envelopePurposeLabel(impl->mode.purpose()));
    state.setProperty("purpose", envelopePurposeLabel(impl->mode.purpose()));
    state.setProperty(
            "polarity",
            impl->mode.purpose() == EnvelopePurpose::Pitch
                    ? "bipolar"
                    : "unipolar");
    state.setProperty("logarithmic", impl->logarithmic.getToggleState());
    state.setProperty(
            "morphPlaneBounds",
            editorBoundsToVar(impl->presentation.planeBounds(controls)));
    state.setProperty(
            "modeBounds",
            editorBoundsToVar(impl->mode.getBounds().toFloat()));
    state.setProperty(
            "purposeBounds",
            editorBoundsToVar(impl->mode.getBounds().toFloat()));
    Array<var> modeOptions;
    for (const EnvelopePurpose purpose : kEnvelopePurposes) {
        auto* option = new DynamicObject();
        option->setProperty("id", envelopePurposeToString(purpose));
        option->setProperty("label", envelopePurposeLabel(purpose));
        option->setProperty("selected", impl->mode.purpose() == purpose);
        option->setProperty("hovered", impl->mode.isOptionHovered(purpose));
        option->setProperty(
                "bounds",
                editorBoundsToVar(impl->mode.optionBounds(purpose)
                        .translated(
                                static_cast<float>(impl->mode.getX()),
                                static_cast<float>(impl->mode.getY()))));
        modeOptions.add(option);
    }
    state.setProperty("modeOptions", modeOptions);
    state.setProperty(
            "blueMorphBounds",
            editorBoundsToVar(impl->blueMorph.slider.getBounds().toFloat()));
    state.setProperty(
            "actionRowBounds",
            editorBoundsToVar(impl->presentation.actionRow(controls)));
    state.setProperty(
            "fitVerticalBounds",
            editorBoundsToVar(impl->fitVertical.getBounds().toFloat()));
    state.setProperty(
            "fullVerticalBounds",
            editorBoundsToVar(impl->fullVertical.getBounds().toFloat()));
    state.setProperty(
            "vertexParameterBounds",
            editorBoundsToVar(impl->presentation.vertexBounds(controls)));
    Array<var> parameterRails;
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(
                controls, index);
        auto* rail = new DynamicObject();
        rail->setProperty("id", parameters[static_cast<size_t>(index)].id);
        rail->setProperty(
                "bounds",
                editorBoundsToVar(TrimeshSidePanelRenderer::vertexParameterRailBounds(row)));
        parameterRails.add(rail);
    }
    state.setProperty("vertexParameterRails", parameterRails);
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
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(controls, index);
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
            return true;
        }
    }
    return false;
}

bool EnvelopeEditorComponent::handleVertexParameterMouseDown(
        Point<float> position,
        Rectangle<float> controls) {
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(controls, index);
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
            const auto& parameter = parameters[static_cast<size_t>(index)];
            impl->parameterId = parameter.id;
            impl->draggingParameter = true;
            const float value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
            if (parameter.value != value
                    && widget.setSelectedVertexParameter(impl->parameterId, value)) {
                publishCurrentState();
            }
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
    const bool redChanged = static_cast<float>(impl->redMorph.slider.getValue()) != red;
    const bool blueChanged = static_cast<float>(impl->blueMorph.slider.getValue()) != blue;
    if (!redChanged && !blueChanged) {
        return true;
    }

    impl->redMorph.slider.setValue(red, dontSendNotification);
    impl->blueMorph.slider.setValue(blue, dontSendNotification);
    publishCurrentState();
    return true;
}

bool EnvelopeEditorComponent::dragVertexParameter(Point<float> position) {
    const auto parameters = widget.selectedVertexParameters();
    const auto controls = editorControlBounds();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        if (parameters[static_cast<size_t>(index)].id != impl->parameterId) {
            continue;
        }
        const auto row = impl->presentation.vertexParameterRowBounds(
                controls, index);
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
        const float value = jlimit(0.f, 1.f, (position.x - rail.getX()) / rail.getWidth());
        if (parameters[static_cast<size_t>(index)].value != value
                && widget.setSelectedVertexParameter(impl->parameterId, value)) {
            publishCurrentState();
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
