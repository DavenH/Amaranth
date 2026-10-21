#include <array>

#include "Graph/NodeParameterMap.h"
#include "Graph/MeshDimensionality.h"
#include "UI/Editors/MeshDimensionOptions.h"
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
#include "UI/EditorChromeLayout.h"
#include "UI/EffectEnableButton.h"
#include "UI/Editors/MorphKeyboardShortcutHelp.h"
#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyHoverTracker.h"
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
        ,   yellowMorph (owner, "Yellow")
        ,   redMorph    (owner, "Red")
        ,   blueMorph   (owner, "Blue") {
        stylePropertyLabel(timeLabel, "Time");
        stylePropertyLabel(purposeLabel, "Purpose");
        purposeLabel.setJustificationType(Justification::centredRight);
        yellowMorph.slider.setMorphPresentation(Colour(0xffe0c247));
        yellowMorph.slider.setComponentID("envelopeEditor.morph.yellow");
        redMorph.slider.setMorphPresentation(Colour(0xffd65a5a));
        blueMorph.slider.setMorphPresentation(Colour(0xff5f91e8));
        redMorph.slider.setComponentID("envelopeEditor.morph.red");
        blueMorph.slider.setComponentID("envelopeEditor.morph.blue");
        owner.addAndMakeVisible(timeLabel);
        owner.addAndMakeVisible(purposeLabel);
        owner.addAndMakeVisible(mode);
        owner.addAndMakeVisible(axisScale);
        owner.addAndMakeVisible(loop);
        owner.addAndMakeVisible(sustain);
        owner.addAndMakeVisible(fitVertical);
        owner.addAndMakeVisible(fullVertical);
        keyboardShortcutHelp = std::make_unique<MorphKeyboardShortcutHelp>(
                owner,
                "envelopeEditor.morph.shortcuts",
                true);
    }

    LabeledParameterSlider& morph(int axis) {
        return axis == 0 ? yellowMorph : axis == 1 ? redMorph : blueMorph;
    }

    bool& linked(int axis) {
        return axis == 0 ? yellowLinked : axis == 1 ? redLinked : blueLinked;
    }

    PropertySegmentedSelector dimensionSelector { meshDimensionOptions(MorphDimensions::Plane::RedBlue) };
    Label dimensionLabel;
    EffectEnableButton enabled;
    LabeledParameterSlider yellowMorph;
    LabeledParameterSlider redMorph;
    LabeledParameterSlider blueMorph;
    Label timeLabel;
    Label purposeLabel;
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
    std::unique_ptr<MorphKeyboardShortcutHelp> keyboardShortcutHelp;
    EnvelopeMorphControls presentation;

    EnvelopePurpose appliedPurpose { EnvelopePurpose::Control };
    int viewAxis {};
    bool hasAppliedPurpose {};
    bool yellowLinked { true };
    bool redLinked { true };
    bool blueLinked { true };
    String linkNodeId;
    String keyboardMorphAxis;
    bool draggingMorph {};
    bool draggingParameter {};
    float guideGainDragStartY {};
    float guideGainDragStartValue {};
    String parameterId;
    String feedbackProperty;
    std::unique_ptr<PropertyHoverTracker> propertyHover;
};

EnvelopeEditorComponent::EnvelopeEditorComponent(CurveEditorWidget& target) :
        CurveExpandedEditorComponent(target)
    ,   impl(std::make_unique<Impl>(*this)) {
    impl->dimensionSelector.setComponentID("envelopeEditor.dimensions");
    impl->dimensionSelector.onChange = [this](const String& count) {
        if (!syncingControls) {
            setNodeParameterText("dimensionCount", "Dimensions", count);
        }
    };
    impl->dimensionSelector.setHoverMessageProvider([this](const String& target) {
        return meshDimensionTooltip(NodeParameterMap(node).intValue("dimensionCount", 0),
                target.getIntValue(), MorphDimensions::Plane::RedBlue);
    });
    addAndMakeVisible(impl->dimensionSelector);
    stylePropertyLabel(impl->dimensionLabel, "Curve morphing");
    addAndMakeVisible(impl->dimensionLabel);
    bindContinuousControls({ &impl->yellowMorph, &impl->redMorph, &impl->blueMorph });
    impl->yellowMorph.slider.setKeyboardShortcutHandler([this](const KeyPress& key) {
        return keyPressed(key);
    });
    impl->yellowMorph.slider.setKeyboardTargetHandler([this] {
        rememberMorphFocus("yellow");
    });
    impl->redMorph.slider.setKeyboardShortcutHandler([this](const KeyPress& key) {
        return keyPressed(key);
    });
    impl->blueMorph.slider.setKeyboardShortcutHandler([this](const KeyPress& key) {
        return keyPressed(key);
    });
    impl->redMorph.slider.setKeyboardTargetHandler([this] {
        rememberMorphFocus("red");
    });
    impl->blueMorph.slider.setKeyboardTargetHandler([this] {
        rememberMorphFocus("blue");
    });
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
    impl->propertyHover = std::make_unique<PropertyHoverTracker>(*this, [this](Point<float> position) {
        if (!impl->draggingParameter) {
            updatePropertyFeedback(position);
        }
    });
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
    const auto controls = editorControlBounds();
    for (const auto& parameter : widget.selectedVertexParameters()) {
        const auto row = impl->presentation.vertexParameterRowBounds(controls, parameter.id);
        if (row.contains(position)) {
            if (parameter.enabled && TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameter.id)
                    && TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).contains(position)) {
                return "Choose a Guide curve for " + parameter.label + ".";
            }
            const bool gain = TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row).contains(position)
                    && TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameter.id);
            return TrimeshSidePanelRenderer::propertyTooltip(widget.selectedVertexParameters(),
                    gain ? "guideGain." + parameter.id.fromLastOccurrenceOf(".", false, false) : parameter.id);
        }
    }
    return {};
}

bool EnvelopeEditorComponent::keyPressed(const KeyPress& key) {
    if (key.getKeyCode() == KeyPress::spaceKey) {
        return false;
    }
    return handleMorphKeyboardShortcut(key);
}

String EnvelopeEditorComponent::focusedMorphAxis() const {
    return impl->keyboardMorphAxis;
}

bool EnvelopeEditorComponent::handleMorphKeyboardShortcut(const KeyPress& key) {
    const String axis = morphAxisForKeyboardShortcut(key);
    if (axis.isNotEmpty()) {
        return selectMorphAxis(axis);
    }
    if (const auto direction = morphAxisNavigationForKeyboardShortcut(key)) {
        return selectAdjacentMorphAxis(*direction);
    }

    PrecisionSlider* slider = focusedMorphSlider();
    if (slider == nullptr) {
        return false;
    }
    if (const auto pole = morphPoleForKeyboardShortcut(key)) {
        slider->setValueFromKeyboard(*pole);
        return true;
    }
    if (const auto amount = morphStepForKeyboardShortcut(key)) {
        slider->setValueFromKeyboard(jlimit(
                0.0,
                1.0,
                slider->getValue() + *amount));
        return true;
    }
    if (isMorphLinkKeyboardShortcut(key)) {
        bool& linked = impl->linked(slider == &impl->yellowMorph.slider ? 0
                : slider == &impl->redMorph.slider ? 1 : 2);
        linked = !linked;
        widget.setEnvelopeDimensions(MeshDimensionality::dimensions(node).axisCount(), impl->yellowLinked);
        widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
        requestRepaint();
        return true;
    }
    return false;
}

void EnvelopeEditorComponent::rememberMorphFocus(const String& axis) {
    if (!selectableMorphAxis(axis)
            || axis == impl->keyboardMorphAxis) {
        return;
    }
    impl->keyboardMorphAxis = axis;
    updateKeyboardShortcutHint();
    repaint();
}

bool EnvelopeEditorComponent::selectMorphAxis(const String& axis) {
    if (!selectableMorphAxis(axis)) {
        return false;
    }
    rememberMorphFocus(axis);
    return true;
}

bool EnvelopeEditorComponent::selectableMorphAxis(const String& axis) const {
    const int dimension = MeshDimensionality::axisForPort(axis);
    return dimension >= 0 && MeshDimensionality::dimensions(node).active(dimension);
}

bool EnvelopeEditorComponent::selectAdjacentMorphAxis(int direction) {
    const std::array<String, 3> axes { "yellow", "red", "blue" };
    int current = -1;
    for (int index = 0; index < 3; ++index) {
        if (axes[(size_t) index] == impl->keyboardMorphAxis) {
            current = index;
        }
    }
    for (int index = current < 0 ? (direction < 0 ? 2 : 0) : current + direction;
            index >= 0 && index < 3; index += direction) {
        if (selectableMorphAxis(axes[(size_t) index])) {
            return selectMorphAxis(axes[(size_t) index]);
        }
    }
    return current >= 0;
}

PrecisionSlider* EnvelopeEditorComponent::focusedMorphSlider() const {
    if (!selectableMorphAxis(impl->keyboardMorphAxis)) {
        return nullptr;
    }
    if (impl->keyboardMorphAxis == "yellow") {
        return &impl->yellowMorph.slider;
    }
    if (impl->keyboardMorphAxis == "red") {
        return &impl->redMorph.slider;
    }
    if (impl->keyboardMorphAxis == "blue") {
        return &impl->blueMorph.slider;
    }
    return nullptr;
}

void EnvelopeEditorComponent::updateKeyboardShortcutHint() {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty()) {
        impl->keyboardShortcutHelp->hide();
        return;
    }

    const int axisIndex = axis == "yellow" ? 0 : axis == "red" ? 1 : 2;
    const Rectangle<float> focus = impl->presentation.keyboardFocusBounds(
            editorControlBounds(),
            axisIndex);
    impl->keyboardShortcutHelp->setAnchorBounds(focus);
}

Rectangle<float> EnvelopeEditorComponent::editorControlBounds() const {
    auto bounds = contentBounds();
    return bounds.removeFromTop(310.f);
}

Rectangle<float> EnvelopeEditorComponent::morphSurfaceBounds() const {
    if (MeshDimensionality::dimensions(node).axisCount() == 0) {
        return {};
    }
    auto bounds = contentBounds().withTrimmedTop(editorControlBounds().getHeight());
    bounds.removeFromLeft(bounds.getWidth() * 0.57f + 8.f);
    return bounds;
}

Rectangle<float> EnvelopeEditorComponent::editorPanelBounds() const {
    auto bounds = contentBounds();
    bounds.removeFromTop(310.f);
    if (MeshDimensionality::dimensions(node).axisCount() > 0) {
        bounds.setWidth(bounds.getWidth() * 0.57f);
    }
    return bounds;
}

void EnvelopeEditorComponent::paintEditor(Graphics& graphics) {
    const auto controls = editorControlBounds();
    const auto dimensions = MeshDimensionality::dimensions(node);
    const int selectedAxis = MorphDimensions::axes[(size_t) impl->viewAxis];
    const int surfaceAxis = dimensions.active(selectedAxis) ? selectedAxis : dimensions.primaryAxis();
    widget.paintEnvelopeMorphSurface(graphics, morphSurfaceBounds(), surfaceAxis,
            { (float) impl->yellowMorph.slider.getValue(),
                    (float) impl->redMorph.slider.getValue(), (float) impl->blueMorph.slider.getValue() });
    impl->presentation.draw(
            graphics,
            controls,
            static_cast<float>(impl->redMorph.slider.getValue()),
            static_cast<float>(impl->blueMorph.slider.getValue()),
            impl->viewAxis,
            impl->redLinked,
            impl->blueLinked,
            impl->loop.getToggleState(),
            impl->sustain.getToggleState(),
            static_cast<float>(impl->yellowMorph.slider.getValue()), impl->yellowLinked, widget.morphPreviewVertices());

    const String focusedAxis = focusedMorphAxis();
    if (focusedAxis.isNotEmpty()) {
        const int axis = focusedAxis == "yellow" ? 0 : focusedAxis == "red" ? 1 : 2;
        paintMorphKeyboardFocus(
                graphics,
                impl->presentation.keyboardFocusBounds(controls, axis));
    }

    const std::array<String, 6> guides = delegate != nullptr
            ? delegate->envelopeGuideLabels(widget.selectedEnvelopeGuideCubeIndex())
            : std::array<String, 6> {};
    TrimeshSidePanelRenderer::drawVertexParameters(
            graphics,
            impl->presentation.vertexBounds(controls),
            widget.selectedVertexParameters(),
            guides,
            EnvelopeMorphControls::vertexParameterHeightScale,
            TrimeshSidePanelRenderer::GuideControls::Visible, 7,
            impl->draggingParameter ? impl->parameterId : impl->feedbackProperty);

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
    const auto header = embeddedEditorHeaderLayout(getLocalBounds().toFloat(), true);
    impl->dimensionLabel.setBounds(Rectangle<float>(92.f, 26.f)
            .withPosition(header.title.getX() + 110.f, header.header.getCentreY() - 13.f).toNearestInt());
    impl->dimensionSelector.setBounds(Rectangle<float>(188.f, 26.f)
            .withPosition(header.title.getX() + 202.f, header.header.getCentreY() - 13.f).toNearestInt());
    impl->mode.setBounds(Rectangle<float>(
            EnvelopeToolbarMetrics::purposeSelectorWidth,
            EnvelopeToolbarMetrics::controlHeight)
            .withCentre({
                    header.enabled.getX()
                            - CanvasChromeMetrics::embeddedEditorActionGap
                            - EnvelopeToolbarMetrics::purposeSelectorWidth * 0.5f,
                    header.header.getCentreY()
            }).toNearestInt());
    impl->purposeLabel.setBounds(
            impl->mode.getX() - 68,
            impl->mode.getY(),
            60,
            impl->mode.getHeight());

    impl->timeLabel.setVisible(false);
    const auto dimensions = MeshDimensionality::dimensions(node);
    for (int axis = 0; axis < 3; ++axis) {
        auto& control = impl->morph(axis);
        const bool active = dimensions.active(MorphDimensions::axes[(size_t) axis]);
        control.label.setVisible(false);
        control.slider.setVisible(active);
        control.setBounds(impl->presentation.morphSliderBounds(controls, axis).toNearestInt(), 42, 0);
    }

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
    updateKeyboardShortcutHint();
}

void EnvelopeEditorComponent::syncEditorFromNode() {
    const EnvelopePurpose purpose = envelopePurposeFor(node);
    const NodeParameterMap parameters(node);
    impl->enabled.setToggleState(
            parameters.boolValue("enabled", true),
            dontSendNotification);
    const auto dimensions = MeshDimensionality::dimensions(node);
    if (!dimensions.active(MorphDimensions::axes[(size_t) impl->viewAxis])) {
        impl->viewAxis = dimensions.axisCount() == 2 ? 1 : 0;
    }
    impl->presentation.setDimensionCount(parameters.intValue("dimensionCount", 0));
    impl->dimensionSelector.setSelectedValue(parameters.stringValue("dimensionCount", "0"));
    impl->yellowMorph.slider.setValue(parameters.floatValue("yellow", 0.f), dontSendNotification);
    impl->mode.setPurpose(purpose);
    impl->redMorph.slider.setValue(parameters.floatValue("red", 0.5f), dontSendNotification);
    impl->blueMorph.slider.setValue(parameters.floatValue("blue", 0.5f), dontSendNotification);
    if (impl->linkNodeId != node.id) {
        impl->linkNodeId = node.id;
        impl->keyboardMorphAxis.clear();
        impl->yellowLinked = true;
        impl->redLinked = true;
        impl->blueLinked = true;
    }
    updateKeyboardShortcutHint();
    const bool logarithmic = parameters.boolValue("logarithmic", false);
    impl->axisScale.setLogarithmic(logarithmic, dontSendNotification);
    impl->axisScale.setEnabled(envelopePurposeAllowsLogarithmic(purpose));
    widget.setEnvelopeDimensions(MeshDimensionality::dimensions(node).axisCount(), impl->yellowLinked);
    widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
    widget.setEnvelopeLogarithmic(logarithmic);
    syncInteractionControls();
}

void EnvelopeEditorComponent::applyEditorStateToWidget() {
    widget.setControlValues(
            true,
            static_cast<float>(impl->redMorph.slider.getValue()),
            static_cast<float>(impl->blueMorph.slider.getValue()),
            static_cast<float>(impl->yellowMorph.slider.getValue()),
            0);
    widget.setEnvelopeDimensions(MeshDimensionality::dimensions(node).axisCount(), impl->yellowLinked);
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

bool EnvelopeEditorComponent::editorInteractionIsSessionOnly(Point<float> position) const {
    const auto controls = editorControlBounds();
    for (int axis = 0; axis < 3; ++axis) {
        if (impl->presentation.axisBounds(controls, axis).contains(position)
                || (impl->presentation.linkBounds(controls, axis).contains(position))) {
            return true;
        }
    }
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < (int) parameters.size(); ++index) {
        if (!parameters[(size_t) index].enabled
                || !TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameters[(size_t) index].id)) {
            continue;
        }
        const auto row = impl->presentation.vertexParameterRowBounds(controls, parameters[(size_t) index].id);
        if (TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).contains(position)) {
            return true;
        }
    }
    return false;
}

std::vector<NodeParameter> EnvelopeEditorComponent::editorControls() const {
    std::vector<NodeParameter> result;
    const EnvelopePurpose purpose = impl->mode.purpose();
    addEditorParameter(result, node, "dimensionCount", "Curve morphing",
            retainedEditorParameter(node, "dimensionCount", "0"));
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
    addEditorParameter(
            result,
            node,
            "declick",
            "Declick",
            retainedEditorParameter(node, "declick", "0"));
    addEditorParameter(result, node, "yellow", "Yellow Morph", String(impl->yellowMorph.slider.getValue()));
    addEditorParameter(result, node, "red", "Red Morph", String(impl->redMorph.slider.getValue()));
    addEditorParameter(result, node, "blue", "Blue Morph", String(impl->blueMorph.slider.getValue()));
    addEditorParameter(result, node, "level", "Level", retainedEditorParameter(node, "level", "1"));
    return result;
}

void EnvelopeEditorComponent::appendEditorAutomation(DynamicObject& state) const {
    const auto controls = editorControlBounds();
    const Rectangle<float> redFocus = impl->presentation.keyboardFocusBounds(controls, 1);
    const Rectangle<float> blueFocus = impl->presentation.keyboardFocusBounds(controls, 2);
    state.setProperty("dimensionCount", impl->presentation.getDimensionCount());
    state.setProperty("yellowMorph", impl->yellowMorph.slider.getValue());
    state.setProperty("redMorph", impl->redMorph.slider.getValue());
    state.setProperty("enabled", impl->enabled.getToggleState());
    state.setProperty("blueMorph", impl->blueMorph.slider.getValue());
    state.setProperty("focusedMorphAxis", focusedMorphAxis());
    state.setProperty(
            "redMorphKeyboardIconBounds",
            editorBoundsToVar(keyboardFocusIndicatorBounds(redFocus)));
    state.setProperty(
            "blueMorphKeyboardIconBounds",
            editorBoundsToVar(keyboardFocusIndicatorBounds(blueFocus)));
    state.setProperty(
            "redMorphKeyboardFocusBounds",
            editorBoundsToVar(redFocus));
    state.setProperty(
            "blueMorphKeyboardFocusBounds",
            editorBoundsToVar(blueFocus));
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
            "redLinkBounds",
            editorBoundsToVar(impl->presentation.linkBounds(controls, 1)));
    state.setProperty(
            "blueLinkBounds",
            editorBoundsToVar(impl->presentation.linkBounds(controls, 2)));
    state.setProperty(
            "modeBounds",
            editorBoundsToVar(impl->mode.getBounds().toFloat()));
    state.setProperty(
            "purposeBounds",
            editorBoundsToVar(impl->mode.getBounds().toFloat()));
    state.setProperty(
            "purposeLabelBounds",
            editorBoundsToVar(impl->purposeLabel.getBounds().toFloat()));
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
                "labelBounds",
                editorBoundsToVar(impl->mode.optionLabelBounds(purpose)
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
    state.setProperty("guideControlsVisible", true);
    state.setProperty("selectedGuideCubeIndex", widget.selectedEnvelopeGuideCubeIndex());
    const auto guideLabels = delegate != nullptr
            ? delegate->envelopeGuideLabels(widget.selectedEnvelopeGuideCubeIndex())
            : std::array<String, 6> {};
    Array<var> encodedGuideLabels;
    for (const auto& label : guideLabels) {
        encodedGuideLabels.add(label);
    }
    state.setProperty("guideLabels", encodedGuideLabels);
    Array<var> parameterRails;
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(
                controls, parameters[(size_t) index].id);
        auto* rail = new DynamicObject();
        rail->setProperty("id", parameters[static_cast<size_t>(index)].id);
        rail->setProperty("enabled", parameters[(size_t) index].enabled);
        rail->setProperty("bounds", editorBoundsToVar(
                TrimeshSidePanelRenderer::vertexParameterRailBounds(row)));
        const bool guides = TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameters[(size_t) index].id);
        rail->setProperty("guideBounds", editorBoundsToVar(guides
                ? TrimeshSidePanelRenderer::vertexParameterGuideBounds(row) : Rectangle<float>()));
        rail->setProperty("guideGainBounds", editorBoundsToVar(guides
                ? TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row) : Rectangle<float>()));
        parameterRails.add(rail);
    }
    state.setProperty("vertexParameterRails", parameterRails);
    state.setProperty("propertyFeedback", impl->draggingParameter ? impl->parameterId : impl->feedbackProperty);
}

bool EnvelopeEditorComponent::editorMouseMove(Point<float> position) {
    const auto controls = editorControlBounds();
    bool interactive = impl->presentation.planeBounds(controls).contains(position);
    for (int axis = 0; axis < 3; ++axis) {
        interactive = interactive || impl->presentation.axisBounds(controls, axis).contains(position);
        interactive = interactive
                || (impl->presentation.linkBounds(controls, axis).contains(position));
    }

    interactive = updatePropertyFeedback(position) || interactive;
    setMouseCursor(interactive ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
    return interactive;
}

bool EnvelopeEditorComponent::updatePropertyFeedback(Point<float> position) {
    const auto controls = editorControlBounds();
    bool interactive = false;
    String feedback;
    const auto parameters = widget.selectedVertexParameters();
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto& parameter = parameters[(size_t) index];
        if (!parameter.enabled) {
            continue;
        }
        const bool guides = TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameter.id);
        const auto row = impl->presentation.vertexParameterRowBounds(controls, parameter.id);
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row);
        if (row.contains(position)) {
            feedback = guides && TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row).contains(position)
                    ? "guideGain." + parameter.id.fromLastOccurrenceOf(".", false, false) : parameter.id;
        }

        interactive = interactive
                || (!parameters[(size_t) index].guideOnly
                        && rail.expanded(5.f, 8.f).contains(position))
                || (guides && (TrimeshSidePanelRenderer::vertexParameterGuideBounds(row).contains(position)
                        || TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row).contains(position)));
    }
    if (impl->feedbackProperty != feedback) {
        impl->feedbackProperty = feedback;
        requestRepaint();
    }
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
            rememberMorphFocus(axis == 0 ? "yellow" : axis == 1 ? "red" : "blue");
            impl->viewAxis = axis;
            requestRepaint();
            return true;
        }
        if (impl->presentation.linkBounds(controls, axis).contains(position)) {
            rememberMorphFocus(axis == 0 ? "yellow" : axis == 1 ? "red" : "blue");
            bool& linked = impl->linked(axis);
            linked = !linked;
            widget.setEnvelopeDimensions(MeshDimensionality::dimensions(node).axisCount(), impl->yellowLinked);
            widget.setEnvelopeAxisLinks(impl->redLinked, impl->blueLinked);
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
    for (int index = 0; index < static_cast<int>(parameters.size()); ++index) {
        const auto row = impl->presentation.vertexParameterRowBounds(controls, parameters[(size_t) index].id);
        const auto& parameter = parameters[(size_t) index];
        if (!parameter.enabled) {
            continue;
        }
        const bool guides = TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameter.id);
        const String field = parameter.id.fromLastOccurrenceOf(".", false, false);
        const auto guideBox = TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);
        if (guides && guideBox.contains(position)) {
            if (delegate != nullptr && widget.selectedEnvelopeGuideCubeIndex() >= 0) {
                delegate->showEnvelopeGuideAttachmentMenu(
                        field,
                        localAreaToGlobal(guideBox.toNearestInt()));
            }
            return true;
        }
        const auto gainBox = TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row);
        if (guides && gainBox.contains(position) && widget.selectedEnvelopeGuideCubeIndex() >= 0) {
            impl->parameterId = "guideGain." + field;
            impl->draggingParameter = true;
            impl->guideGainDragStartY = position.y;
            impl->guideGainDragStartValue = parameter.guideGain;
            return true;
        }
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row);
        if (!parameter.guideOnly && rail.expanded(5.f, 8.f).contains(position)) {
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
        const auto& parameter = parameters[(size_t) index];
        const String guideGainId = "guideGain."
                + parameter.id.fromLastOccurrenceOf(".", false, false);
        if (!parameter.enabled || (parameter.id != impl->parameterId && guideGainId != impl->parameterId)
                || (guideGainId == impl->parameterId
                        && !TrimeshSidePanelRenderer::showsGuideControlsForParameter(parameter.id))) {
            continue;
        }
        const auto row = impl->presentation.vertexParameterRowBounds(
                controls, parameters[(size_t) index].id);
        if (impl->parameterId == guideGainId) {
            const float value = jlimit(
                    0.f,
                    1.f,
                    impl->guideGainDragStartValue
                            + (impl->guideGainDragStartY - position.y) / 120.f);
            if (parameter.guideGain != value
                    && widget.setSelectedVertexParameter(impl->parameterId, value)) {
                publishCurrentState();
            }
            return true;
        }
        const auto rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row);
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
