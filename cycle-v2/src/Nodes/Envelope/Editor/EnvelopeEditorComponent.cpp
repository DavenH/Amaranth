#include <array>

#include "Graph/NodeParameterMap.h"
#include "Nodes/Envelope/Editor/EnvelopeEditorComponent.h"
#include "Nodes/Envelope/Editor/EnvelopeAxisScaleSelector.h"
#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/Editor/EnvelopeMorphControls.h"
#include "Nodes/Envelope/EnvelopePurpose.h"
#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EnvelopePurposeSelector.h"
#include "UI/EnvelopeToolbarMetrics.h"
#include "UI/EffectEnableButton.h"
#include "UiIconData.h"

namespace CycleV2 {

namespace {

class EnvelopeActionButton final : public Button {
public:
    EnvelopeActionButton(
            const String& accessibleName,
            const String& tooltip,
            const char* svg) :
            Button (accessibleName)
        ,   icon   (createIcon(svg)) {
        setTitle(accessibleName);
        setDescription(tooltip);
        setTooltip(tooltip);
        setMouseCursor(MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus(true);
    }

    bool hasIcon() const { return icon != nullptr; }

    Rectangle<float> iconCanvasBounds() const {
        return getLocalBounds().toFloat().withSizeKeepingCentre(
                EnvelopeToolbarMetrics::actionIconCanvasSize,
                EnvelopeToolbarMetrics::actionIconCanvasSize);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        const Rectangle<float> bounds = getLocalBounds().toFloat();
        if (down || highlighted) {
            graphics.setColour(Colours::white.withAlpha(down ? 0.12f : 0.06f));
            graphics.fillRect(bounds.reduced(2.f));
        }
        if (icon != nullptr) {
            const float opacity = isEnabled()
                    ? getToggleState() ? 1.f : highlighted ? 0.9f : 0.72f
                    : 0.25f;
            icon->drawWithin(
                    graphics,
                    iconCanvasBounds(),
                    RectanglePlacement::centred,
                    opacity);
        }
        if (hasKeyboardFocus(false)) {
            graphics.setColour(Colour(0xff65b8ff));
            graphics.drawRoundedRectangle(
                    bounds.reduced(1.f),
                    CanvasChromeMetrics::controlCornerRadius,
                    CanvasChromeMetrics::focusRingWidth);
        }
    }

private:
    static std::unique_ptr<Drawable> createIcon(const char* svg) {
        const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
        jassert(document != nullptr);
        return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
    }

    std::unique_ptr<Drawable> icon;
};

}

struct EnvelopeEditorComponent::Impl {
    explicit Impl(Component& owner) :
            enabled     ("Envelope enabled",
                         "Toggles this Envelope layer",
                         "Enable or disable this Envelope layer")
        ,   redMorph    (owner, "Red")
        ,   blueMorph   (owner, "Blue")
        ,   tooltipHost (&owner, 500) {
        stylePropertyLabel(timeLabel, "Time");
        redMorph.slider.setMorphPresentation(Colour(0xffd65a5a));
        blueMorph.slider.setMorphPresentation(Colour(0xff5f91e8));
        owner.addAndMakeVisible(timeLabel);
        owner.addAndMakeVisible(mode);
        owner.addAndMakeVisible(axisScale);
        owner.addAndMakeVisible(loop);
        owner.addAndMakeVisible(sustain);
        owner.addAndMakeVisible(fitVertical);
        owner.addAndMakeVisible(fullVertical);
    }

    EffectEnableButton enabled;
    LabeledParameterSlider redMorph;
    LabeledParameterSlider blueMorph;
    TooltipWindow tooltipHost;
    Label timeLabel;
    EnvelopePurposeSelector mode;
    EnvelopeAxisScaleSelector axisScale;
    EnvelopeActionButton loop {
            "Set selected vertex as loop start",
            "Select one envelope vertex to set the loop start",
            UiIconData::envelopeLoop
    };
    EnvelopeActionButton sustain {
            "Set selected vertex as sustain point",
            "Select one envelope vertex to set the sustain point",
            UiIconData::envelopeSustain
    };
    EnvelopeActionButton fitVertical {
            "Fit envelope vertical range",
            "Fit the vertical zoom to the envelope",
            UiIconData::envelopeZoomFit
    };
    EnvelopeActionButton fullVertical {
            "Show full envelope vertical range",
            "Reset the vertical zoom to the full range",
            UiIconData::envelopeZoomFull
    };
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

EnvelopeEditorComponent::EnvelopeEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    bindContinuousControls({ &impl->redMorph, &impl->blueMorph });
    impl->enabled.setComponentID("envelopeEditor.enabled");
    setHeaderAction(impl->enabled);
    bindDiscreteAction(impl->enabled, [] {});
    impl->mode.onChange = [this](EnvelopePurpose) {
        publishDiscreteControlChange();
    };

    bindDiscreteAction(impl->loop, [this] {
        widget.toggleSelectedEnvelopeMarker(true);
        syncInteractionControls();
    });
    bindDiscreteAction(impl->sustain, [this] {
        widget.toggleSelectedEnvelopeMarker(false);
        syncInteractionControls();
    });
    impl->axisScale.onChange = [this](bool) {
        publishDiscreteControlChange();
    };
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

String EnvelopeEditorComponent::getTooltip() {
    const Point<float> position = getLocalPoint(
            nullptr, Desktop::getMousePosition()).toFloat();
    if (!impl->loop.isEnabled()
            && impl->loop.getBounds().toFloat().contains(position)) {
        return impl->loop.getTooltip();
    }
    if (!impl->sustain.isEnabled()
            && impl->sustain.getBounds().toFloat().contains(position)) {
        return impl->sustain.getTooltip();
    }
    return {};
}

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
            impl->blueLinked,
            impl->loop.getToggleState(),
            impl->sustain.getToggleState());

    std::array<String, 6> guides {};
    TrimeshSidePanelRenderer::drawVertexParameters(
            graphics,
            impl->presentation.vertexBounds(controls),
            widget.selectedVertexParameters(),
            guides,
            EnvelopeMorphControls::vertexParameterHeightScale,
            TrimeshSidePanelRenderer::GuideControls::Hidden);

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
    impl->mode.setBounds(
            impl->presentation.purposeSelectorBounds(controls).toNearestInt());

    auto timeRow = impl->presentation.morphRow(controls, 0).toNearestInt();
    impl->timeLabel.setBounds(timeRow.removeFromLeft(42));

    auto redRow = impl->presentation.morphRow(controls, 1).toNearestInt();
    redRow.removeFromRight(58);
    impl->redMorph.setBounds(redRow, 42, 0);

    auto blueRow = impl->presentation.morphRow(controls, 2).toNearestInt();
    blueRow.removeFromRight(58);
    impl->blueMorph.setBounds(blueRow, 42, 0);

    auto markerGroup = impl->presentation.markerGroupBounds(controls).toNearestInt();
    const int markerWidth = markerGroup.getWidth() / 2;
    const int pairedActionInset = roundToInt(
            EnvelopeToolbarMetrics::pairedActionOuterInset);
    impl->loop.setBounds(
            markerGroup.removeFromLeft(markerWidth).reduced(pairedActionInset, 0));
    impl->sustain.setBounds(markerGroup.reduced(pairedActionInset, 0));
    impl->axisScale.setBounds(
            impl->presentation.axisScaleBounds(controls).toNearestInt());

    auto rangeGroup = impl->presentation.rangeGroupBounds(controls).toNearestInt();
    const int rangeWidth = rangeGroup.getWidth() / 2;
    impl->fitVertical.setBounds(
            rangeGroup.removeFromLeft(rangeWidth).reduced(pairedActionInset, 0));
    impl->fullVertical.setBounds(rangeGroup.reduced(pairedActionInset, 0));
}

void EnvelopeEditorComponent::syncEditorFromNode() {
    EnvelopeNodeModel model;
    model.syncFromNode(node);
    const EnvelopePurpose purpose = envelopePurposeFor(node);
    impl->enabled.setToggleState(
            NodeParameterMap(node).boolValue("enabled", true),
            dontSendNotification);
    impl->mode.setPurpose(purpose);
    impl->redMorph.slider.setValue(model.red, dontSendNotification);
    impl->blueMorph.slider.setValue(model.blue, dontSendNotification);
    impl->redLinked = model.redLinked;
    impl->blueLinked = model.blueLinked;
    impl->axisScale.setLogarithmic(model.logarithmic, dontSendNotification);
    impl->axisScale.setEnabled(envelopePurposeAllowsLogarithmic(purpose));
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
    widget.setEnvelopeLogarithmic(model.logarithmic);
    syncInteractionControls();
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
                    && impl->axisScale.isLogarithmic());
    impl->axisScale.setEnabled(envelopePurposeAllowsLogarithmic(purpose));
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
            "enabled",
            "Enabled",
            impl->enabled.getToggleState() ? "1" : "0");
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
                    && impl->axisScale.isLogarithmic() ? "1" : "0");
    addEditorParameter(result, node, "red", "Red Morph", String(impl->redMorph.slider.getValue()));
    addEditorParameter(result, node, "blue", "Blue Morph", String(impl->blueMorph.slider.getValue()));
    addEditorParameter(result, node, "level", "Level", retainedEditorParameter(node, "level", "1"));
    return result;
}

void EnvelopeEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    const auto controls = editorControlBounds();
    state.setProperty("redMorph", impl->redMorph.slider.getValue());
    state.setProperty("enabled", impl->enabled.getToggleState());
    state.setProperty("blueMorph", impl->blueMorph.slider.getValue());
    state.setProperty("viewAxis", impl->viewAxis);
    state.setProperty("modeLabel", "Purpose");
    state.setProperty("mode", envelopePurposeLabel(impl->mode.purpose()));
    state.setProperty("purpose", envelopePurposeLabel(impl->mode.purpose()));
    state.setProperty(
            "polarity",
            impl->mode.purpose() == EnvelopePurpose::Pitch
                    ? "bipolar"
                    : "unipolar");
    state.setProperty("logarithmic", impl->axisScale.isLogarithmic());
    state.setProperty(
            "purposeGroupLabelBounds",
            editorBoundsToVar(impl->presentation.purposeGroupLabelBounds(controls)));
    state.setProperty(
            "morphGroupLabelBounds",
            editorBoundsToVar(impl->presentation.morphGroupLabelBounds(controls)));
    state.setProperty(
            "morphPlaneGroupLabelBounds",
            editorBoundsToVar(impl->presentation.planeGroupLabelBounds(controls)));
    state.setProperty(
            "morphPlaneBounds",
            editorBoundsToVar(impl->presentation.planeBounds(controls)));
    state.setProperty(
            "axisGroupLabelBounds",
            editorBoundsToVar(impl->presentation.axisGroupLabelBounds(controls)));
    state.setProperty(
            "linkGroupLabelBounds",
            editorBoundsToVar(impl->presentation.linkGroupLabelBounds(controls)));
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
        option->setProperty(
                "iconBounds",
                editorBoundsToVar(impl->mode.optionIconBounds(purpose)
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
            "actionBarBounds",
            editorBoundsToVar(impl->presentation.actionBarBounds(controls)));
    state.setProperty("markerGroupLabel", "Markers");
    state.setProperty("axisScaleGroupLabel", "Scaling");
    state.setProperty("rangeGroupLabel", "Zoom");
    state.setProperty(
            "actionIconsVector",
            impl->loop.hasIcon()
                    && impl->sustain.hasIcon()
                    && impl->fitVertical.hasIcon()
                    && impl->fullVertical.hasIcon());
    state.setProperty(
            "markerGroupLabelBounds",
            editorBoundsToVar(impl->presentation.markerGroupLabelBounds(controls)));
    state.setProperty(
            "markerGroupBounds",
            editorBoundsToVar(impl->presentation.markerGroupBounds(controls)));
    state.setProperty("loopBounds", editorBoundsToVar(impl->loop.getBounds().toFloat()));
    state.setProperty("sustainBounds", editorBoundsToVar(impl->sustain.getBounds().toFloat()));
    const auto actionIconBounds = [](const EnvelopeActionButton& button) {
        return button.iconCanvasBounds().translated(
                static_cast<float>(button.getX()),
                static_cast<float>(button.getY()));
    };
    state.setProperty(
            "loopIconBounds",
            editorBoundsToVar(actionIconBounds(impl->loop)));
    state.setProperty(
            "sustainIconBounds",
            editorBoundsToVar(actionIconBounds(impl->sustain)));
    state.setProperty("loopEnabled", impl->loop.isEnabled());
    state.setProperty("sustainEnabled", impl->sustain.isEnabled());
    state.setProperty("loopTooltip", impl->loop.getTooltip());
    state.setProperty("sustainTooltip", impl->sustain.getTooltip());
    state.setProperty(
            "axisScaleGroupLabelBounds",
            editorBoundsToVar(impl->presentation.axisScaleGroupLabelBounds(controls)));
    state.setProperty(
            "axisScaleBounds",
            editorBoundsToVar(impl->axisScale.getBounds().toFloat()));
    state.setProperty(
            "linearAxisScaleBounds",
            editorBoundsToVar(impl->axisScale.optionBounds(false)
                    .translated(
                            static_cast<float>(impl->axisScale.getX()),
                            static_cast<float>(impl->axisScale.getY()))));
    state.setProperty(
            "logarithmicAxisScaleBounds",
            editorBoundsToVar(impl->axisScale.optionBounds(true)
                    .translated(
                            static_cast<float>(impl->axisScale.getX()),
                            static_cast<float>(impl->axisScale.getY()))));
    Array<var> axisScaleOptions;
    for (bool logarithmic : { false, true }) {
        auto* option = new DynamicObject();
        option->setProperty("label", logarithmic ? "Logarithmic" : "Linear");
        option->setProperty("selected", impl->axisScale.isLogarithmic() == logarithmic);
        option->setProperty(
                "bounds",
                editorBoundsToVar(impl->axisScale.optionBounds(logarithmic)
                        .translated(
                                static_cast<float>(impl->axisScale.getX()),
                                static_cast<float>(impl->axisScale.getY()))));
        option->setProperty(
                "diagramBounds",
                editorBoundsToVar(impl->axisScale.optionDiagramBounds(logarithmic)
                        .translated(
                                static_cast<float>(impl->axisScale.getX()),
                                static_cast<float>(impl->axisScale.getY()))));
        axisScaleOptions.add(option);
    }
    state.setProperty("axisScaleOptions", axisScaleOptions);
    state.setProperty(
            "rangeGroupLabelBounds",
            editorBoundsToVar(impl->presentation.rangeGroupLabelBounds(controls)));
    state.setProperty(
            "rangeGroupBounds",
            editorBoundsToVar(impl->presentation.rangeGroupBounds(controls)));
    state.setProperty(
            "fitVerticalBounds",
            editorBoundsToVar(impl->fitVertical.getBounds().toFloat()));
    state.setProperty(
            "fitVerticalIconBounds",
            editorBoundsToVar(actionIconBounds(impl->fitVertical)));
    state.setProperty(
            "fullVerticalBounds",
            editorBoundsToVar(impl->fullVertical.getBounds().toFloat()));
    state.setProperty(
            "fullVerticalIconBounds",
            editorBoundsToVar(actionIconBounds(impl->fullVertical)));
    state.setProperty(
            "vertexParameterBounds",
            editorBoundsToVar(impl->presentation.vertexBounds(controls)));
    state.setProperty("guideControlsVisible", false);
    Array<var> parameterRails;
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(
                controls, index);
        auto* rail = new DynamicObject();
        rail->setProperty("id", parameters[static_cast<size_t>(index)].id);
        rail->setProperty(
                "bounds",
                editorBoundsToVar(TrimeshSidePanelRenderer::vertexParameterRailBounds(
                        row,
                        TrimeshSidePanelRenderer::GuideControls::Hidden)));
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
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row,
                TrimeshSidePanelRenderer::GuideControls::Hidden);
        interactive = interactive || rail.expanded(5.f, 8.f).contains(position);
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
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row,
                TrimeshSidePanelRenderer::GuideControls::Hidden);
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
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row,
                TrimeshSidePanelRenderer::GuideControls::Hidden);
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

void EnvelopeEditorComponent::syncInteractionControls() {
    const bool hasSelectedVertex = widget.hasSingleSelectedEnvelopeVertex();
    impl->loop.setEnabled(hasSelectedVertex);
    impl->sustain.setEnabled(hasSelectedVertex);
    impl->loop.setTooltip(hasSelectedVertex
            ? "Toggle selected vertex as loop start"
            : "Select one envelope vertex to set the loop start");
    impl->sustain.setTooltip(hasSelectedVertex
            ? "Toggle selected vertex as sustain point"
            : "Select one envelope vertex to set the sustain point");
    impl->loop.setToggleState(
            hasSelectedVertex && widget.selectedEnvelopeMarkerState(true),
            dontSendNotification);
    impl->sustain.setToggleState(
            hasSelectedVertex && widget.selectedEnvelopeMarkerState(false),
            dontSendNotification);
}

}
