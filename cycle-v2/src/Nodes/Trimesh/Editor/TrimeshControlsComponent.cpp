#include "Nodes/Trimesh/Editor/TrimeshControlsComponent.h"
#include "Graph/NodeParameterMap.h"
#include "Graph/MeshDimensionality.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

#include "UI/Editors/MorphKeyboardShortcutHelp.h"
#include "UI/Editors/MorphKeyboardShortcuts.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyHoverTracker.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

class TrimeshControlsComponent::ControlTarget : public Component, public TooltipClient {
public:
    ControlTarget(
            TrimeshControlsComponent& targetOwner,
            TrimeshExpandedHitRegion targetRegion) :
            owner   (targetOwner)
        ,   region  (std::move(targetRegion)) {
        setOpaque(false);
        setMouseCursor(
                region.kind == TrimeshExpandedHitRegionKind::MorphControl
                        || region.kind == TrimeshExpandedHitRegionKind::OutputScale
                        || region.kind == TrimeshExpandedHitRegionKind::VertexParameter
                        || region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
                ? (region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
                        ? MouseCursor::UpDownResizeCursor
                        : MouseCursor::LeftRightResizeCursor)
                : MouseCursor::PointingHandCursor);
        if (region.kind == TrimeshExpandedHitRegionKind::LinkToggle) {
            setComponentID("trimesh.link." + region.axisValue);
            setTitle("Edit both " + region.axisValue + " poles together");
            setWantsKeyboardFocus(true);
        }
        if (region.kind == TrimeshExpandedHitRegionKind::MorphControl) {
            setComponentID("trimesh.morph." + region.parameterId);
            setTitle(region.parameterId + " morph");
            setWantsKeyboardFocus(true);
        }
        if (region.kind == TrimeshExpandedHitRegionKind::OutputScale) {
            setComponentID("trimeshEditor." + region.parameterId);
            setTitle(region.parameterId == "gain" ? "Output gain" : "Spectral range");
        }
        if (region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain) {
            setComponentID("trimesh.guideGain." + region.parameterId.fromLastOccurrenceOf(
                    ".", false, false));
            setTitle("Guide curve gain");
            setWantsKeyboardFocus(true);
        }
        if (region.kind == TrimeshExpandedHitRegionKind::VertexParameter) {
            setComponentID("trimesh.vertex." + region.parameterId.fromLastOccurrenceOf(
                    ".", false, false));
            setTitle("Vertex " + region.parameterId.fromLastOccurrenceOf(
                    ".", false, false));
        }
        if (region.kind == TrimeshExpandedHitRegionKind::VertexGuideAttachment) {
            setComponentID("trimesh.guide." + region.parameterId.fromLastOccurrenceOf(
                    ".", false, false));
            setTitle("Vertex guide curve");
        }
        setName(region.kind == TrimeshExpandedHitRegionKind::OutputScale
                ? "TrimeshOutputScaleTarget"
                : "TrimeshControlTarget");
    }

    String getTooltip() override { return owner.tooltipFor(region); }

    void paint(Graphics& graphics) override {
        if (region.kind == TrimeshExpandedHitRegionKind::LinkToggle) {
            const bool linked = NodeParameterMap(owner.node).boolValue("link." + region.axisValue, true);
            const MorphDimension dimension = region.axisValue == "yellow" ? MorphDimension::Yellow
                    : region.axisValue == "red" ? MorphDimension::Red : MorphDimension::Blue;
            const Colour colour = colourForMorphDimension(dimension);
            paintMorphAxisLock(graphics, getLocalBounds().toFloat(), colour, linked,
                    isMouseOverOrDragging(), hasKeyboardFocus(false));
        }
    }

    void mouseEnter(const MouseEvent&) override {
        owner.showPropertyFeedback(region.parameterId);
        repaint();
    }

    void mouseExit(const MouseEvent&) override {
        repaint();
        if (owner.dragTarget == DragTarget::None) {
            owner.showPropertyFeedback({});
        }
    }

    void mouseDown(const MouseEvent& event) override {
        owner.beginControlDrag(region, ownerPosition(event.position), getScreenBounds());
    }

    void mouseDrag(const MouseEvent& event) override {
        owner.dragControl(ownerPosition(event.position));
    }

    void mouseUp(const MouseEvent&) override {
        owner.endControlDrag();
    }

    bool keyPressed(const KeyPress& key) override {
        if (key.getKeyCode() == KeyPress::spaceKey) {
            return false;
        }
        if (region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
                && (key.getKeyCode() == KeyPress::upKey
                        || key.getKeyCode() == KeyPress::rightKey
                        || key.getKeyCode() == KeyPress::downKey
                        || key.getKeyCode() == KeyPress::leftKey)) {
            const bool increases = key.getKeyCode() == KeyPress::upKey
                    || key.getKeyCode() == KeyPress::rightKey;
            return owner.adjustGuideGainFromKeyboard(region, increases ? 0.01f : -0.01f);
        }
        if (owner.keyPressed(key)) {
            return true;
        }
        if (region.kind == TrimeshExpandedHitRegionKind::LinkToggle
                && key.getKeyCode() == KeyPress::returnKey) {
            owner.beginControlDrag(region, region.bounds.getCentre(), getScreenBounds());
            return true;
        }
        return Component::keyPressed(key);
    }

    void focusGained(FocusChangeType cause) override {
        if (cause == focusChangedByTabKey) {
            owner.rememberMorphFocus(region);
        }
        owner.repaint();
    }

    void focusLost(FocusChangeType) override {
        owner.repaint();
    }

private:
    Point<float> ownerPosition(Point<float> position) const {
        return getBounds().toFloat().getTopLeft() + position;
    }

    TrimeshControlsComponent& owner;
    TrimeshExpandedHitRegion region;
};

TrimeshControlsComponent::TrimeshControlsComponent(TrimeshWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(false, true);
    keyboardShortcutHelp = std::make_unique<MorphKeyboardShortcutHelp>(
            *this,
            "trimesh.morph.shortcuts",
            true);
    propertyHover = std::make_unique<PropertyHoverTracker>(*this, [this](Point<float> position) {
        if (dragTarget != DragTarget::None) {
            return;
        }
        String feedback;
        for (const auto& region : controlHitRegions) {
            if (region.bounds.contains(position) && isControlEnabled(region)
                    && (region.kind == TrimeshExpandedHitRegionKind::VertexParameter
                            || region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain)) {
                feedback = region.parameterId;
                break;
            }
        }
        showPropertyFeedback(feedback);
    });
}

TrimeshControlsComponent::~TrimeshControlsComponent() = default;

void TrimeshControlsComponent::showPropertyFeedback(const String& id) {
    if (widget.propertyFeedbackId() != id) {
        widget.setPropertyFeedback(id);
        if (auto* parent = getParentComponent()) {
            parent->repaint();
        }
    }
}

String TrimeshControlsComponent::tooltipFor(const TrimeshExpandedHitRegion& region) {
    if (region.kind == TrimeshExpandedHitRegionKind::LinkToggle) {
        return morphAxisLinkTooltip(region.axisValue.substring(0, 1).toUpperCase() + region.axisValue.substring(1),
                NodeParameterMap(node).boolValue("link." + region.axisValue, true));
    }
    const bool vertex = region.kind == TrimeshExpandedHitRegionKind::VertexParameter
            || region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
            || region.kind == TrimeshExpandedHitRegionKind::VertexGuideAttachment;
    if (vertex && widget.selectedVertexIndexForPanel() < 0) {
        return "Select a curve point to edit its properties.";
    }
    if (vertex && !TrimeshWidget::vertexPropertyActive(region.parameterId,
            NodeParameterMap(node).intValue("dimensionCount", 0))) {
        return "Unavailable in the current Curve morphing mode.";
    }
    if (region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
            && !widget.hasGuideAttachmentForParameter(region.parameterId)) {
        return "Attach a Guide curve to adjust its gain.";
    }
    if (region.kind == TrimeshExpandedHitRegionKind::VertexGuideAttachment) {
        return "Choose a Guide curve for this component.";
    }
    return TrimeshSidePanelRenderer::propertyTooltip(widget.selectedVertexParameters(), region.parameterId);
}

String TrimeshControlsComponent::getTooltip() {
    const auto position = getMouseXYRelative().toFloat();
    for (const auto& region : controlHitRegions) {
        if (region.bounds.contains(position)) {
            return tooltipFor(region);
        }
    }
    return {};
}

void TrimeshControlsComponent::setDelegate(TrimeshControlsDelegate* nextDelegate) {
    delegate = nextDelegate;
}

void TrimeshControlsComponent::setNode(const Node& nextNode) {
    if (NodeParameterMap(node).intValue("dimensionCount", 0)
            != NodeParameterMap(nextNode).intValue("dimensionCount", 0)) {
        lastHitRegionContentBounds = {};
    }
    if (!keyboardMorphAxis.isEmpty()
            && !MeshDimensionality::inputActive(nextNode, keyboardMorphAxis)) {
        keyboardMorphAxis.clear();
    }
    if (node.id != nextNode.id) {
        keyboardMorphAxis.clear();
    }
    node = nextNode;
    updateHitRegions();
}

void TrimeshControlsComponent::setContentBounds(Rectangle<float> nextContentBounds) {
    contentBounds = nextContentBounds;
    updateHitRegions();
}

void TrimeshControlsComponent::refreshHitRegions() {
    lastHitRegionContentBounds = {};
    updateHitRegions();
}

void TrimeshControlsComponent::refreshSelectionState() {
    updateControlEnabledStates();
}

int TrimeshControlsComponent::getPrimaryAxisButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::PrimaryAxis);
}

int TrimeshControlsComponent::getMorphSliderCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::MorphControl);
}

int TrimeshControlsComponent::getOutputScaleSliderCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::OutputScale);
}

int TrimeshControlsComponent::getLinkToggleButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::LinkToggle);
}

int TrimeshControlsComponent::getVertexParameterSliderCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::VertexParameter);
}

int TrimeshControlsComponent::getVertexGuideGainKnobCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::VertexGuideGain);
}

int TrimeshControlsComponent::getVertexGuideAttachmentButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::VertexGuideAttachment);
}

String TrimeshControlsComponent::focusedMorphAxis() const {
    return keyboardMorphAxis;
}

void TrimeshControlsComponent::paint(Graphics& graphics) {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty()) {
        return;
    }

    const int axisIndex = axis == "yellow" ? 0 : axis == "red" ? 1 : 2;
    paintMorphKeyboardFocus(
            graphics,
            TrimeshWidget::expandedMorphKeyboardFocusBounds(
                    contentBounds,
                    axisIndex,
                    widget.showsOutputScale(),
                    MeshDimensionality::dimensions(node).axisCount()));
}

void TrimeshControlsComponent::resized() {
    updateHitRegions();
}

bool TrimeshControlsComponent::keyPressed(const KeyPress& key) {
    const String axis = morphAxisForKeyboardShortcut(key);
    if (axis.isNotEmpty()) {
        return selectMorphControl(axis);
    }
    if (const auto direction = morphAxisNavigationForKeyboardShortcut(key)) {
        return selectAdjacentMorphControl(*direction);
    }
    if (const auto pole = morphPoleForKeyboardShortcut(key)) {
        return setFocusedMorphTo(*pole);
    }
    if (const auto amount = morphStepForKeyboardShortcut(key)) {
        return adjustFocusedMorphBy(*amount);
    }
    if (isMorphLinkKeyboardShortcut(key)) {
        return toggleFocusedMorphLink();
    }
    return false;
}

MouseCursor TrimeshControlsComponent::cursorFor(Point<float> position) {
    const auto* region = findControlRegion(position);

    if (region != nullptr) {
        if (region->kind == TrimeshExpandedHitRegionKind::MorphControl
                || region->kind == TrimeshExpandedHitRegionKind::OutputScale
                || region->kind == TrimeshExpandedHitRegionKind::VertexParameter) {
            return MouseCursor::LeftRightResizeCursor;
        }
        if (region->kind == TrimeshExpandedHitRegionKind::VertexGuideGain) {
            return widget.hasGuideAttachmentForParameter(region->parameterId)
                    ? MouseCursor::UpDownResizeCursor
                    : MouseCursor::NormalCursor;
        }

        return MouseCursor::PointingHandCursor;
    }

    int vertexIndex {};

    if (widget.findVertexSelectionAt(node, contentBounds, position, vertexIndex)) {
        return MouseCursor::PointingHandCursor;
    }

    return MouseCursor::NormalCursor;
}

void TrimeshControlsComponent::beginPointerInteraction(
        Point<float> position,
        Rectangle<int> screenArea) {
    const auto* region = findControlRegion(position);

    if (region != nullptr) {
        beginControlDrag(*region, position, screenArea);
        return;
    }

    int vertexIndex {};

    if (widget.findVertexSelectionAt(node, contentBounds, position, vertexIndex)
            && delegate != nullptr) {
        delegate->selectTrimeshVertex(vertexIndex);
    }
}

void TrimeshControlsComponent::continuePointerInteraction(Point<float> position) {
    dragControl(position);
}

void TrimeshControlsComponent::endPointerInteraction() {
    endControlDrag();
}

void TrimeshControlsComponent::updateHitRegions() {
    const Rectangle<int> nextContentBounds = contentBounds.toNearestInt();

    if (nextContentBounds == lastHitRegionContentBounds && !controlRegions.empty()) {
        updateKeyboardShortcutHint();
        return;
    }

    for (auto& region : controlRegions) {
        removeChildComponent(region.get());
    }

    controlRegions.clear();
    controlHitRegions.clear();
    lastHitRegionContentBounds = nextContentBounds;

    if (node.kind != NodeKind::TrilinearMesh || contentBounds.isEmpty()) {
        updateKeyboardShortcutHint();
        return;
    }

    controlHitRegions = widget.expandedControlHitRegions(
            contentBounds,
            widget.showsOutputScale(),
            widget.outputScaleParameterId(),
            NodeParameterMap(node).intValue("dimensionCount", 0));

    for (const auto& region : controlHitRegions) {
        auto component = std::make_unique<ControlTarget>(*this, region);
        component->setBounds(region.bounds.toNearestInt());
        addAndMakeVisible(component.get());
        controlRegions.push_back(std::move(component));
    }
    updateControlEnabledStates();
    updateKeyboardShortcutHint();
}

int TrimeshControlsComponent::countControlRegions(TrimeshExpandedHitRegionKind kind) const {
    return static_cast<int>(std::count_if(
            controlHitRegions.begin(),
            controlHitRegions.end(),
            [kind](const TrimeshExpandedHitRegion& region) {
                return region.kind == kind;
            }));
}

const TrimeshExpandedHitRegion* TrimeshControlsComponent::findControlRegion(Point<float> position) const {
    const auto found = std::find_if(
            controlHitRegions.begin(),
            controlHitRegions.end(),
            [this, position](const TrimeshExpandedHitRegion& region) {
                return isControlEnabled(region) && region.bounds.contains(position);
            });

    if (found == controlHitRegions.end()) {
        return nullptr;
    }

    return &*found;
}

bool TrimeshControlsComponent::isControlEnabled(
        const TrimeshExpandedHitRegion& region) const {
    const bool isVertexControl = region.kind == TrimeshExpandedHitRegionKind::VertexParameter
            || region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
            || region.kind == TrimeshExpandedHitRegionKind::VertexGuideAttachment;
    if (isVertexControl && widget.selectedVertexIndexForPanel() < 0) {
        return false;
    }
    if (isVertexControl && !TrimeshWidget::vertexPropertyActive(
                region.parameterId, NodeParameterMap(node).intValue("dimensionCount", 0))) {
        return false;
    }

    return region.kind != TrimeshExpandedHitRegionKind::VertexGuideGain
            || widget.hasGuideAttachmentForParameter(region.parameterId);
}

void TrimeshControlsComponent::updateControlEnabledStates() {
    for (size_t i = 0; i < controlRegions.size(); ++i) {
        controlRegions[i]->setEnabled(isControlEnabled(controlHitRegions[i]));
    }
}

void TrimeshControlsComponent::beginControlDrag(
        const TrimeshExpandedHitRegion& region,
        Point<float> position,
        Rectangle<int> screenArea) {
    rememberMorphFocus(region);
    showPropertyFeedback(region.parameterId);
    float value {};

    switch (region.kind) {
        case TrimeshExpandedHitRegionKind::PrimaryAxis:
            if (delegate != nullptr) {
                delegate->setTrimeshPrimaryAxis(region.axisValue);
            }
            break;

        case TrimeshExpandedHitRegionKind::LinkToggle:
            if (delegate != nullptr) {
                delegate->toggleTrimeshLinkAxis(region.axisValue);
            }
            break;

        case TrimeshExpandedHitRegionKind::MorphControl:
            dragTarget = DragTarget::Morph;
            activeParameterId = region.parameterId;

            if (widget.morphValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
                delegate->beginTrimeshMorphControlEdit(activeParameterId, value);
            }
            break;

        case TrimeshExpandedHitRegionKind::OutputScale:
            dragTarget = DragTarget::OutputScale;
            activeParameterId = region.parameterId;

            if (widget.outputScaleValueAt(contentBounds, position, value)
                    && delegate != nullptr) {
                delegate->beginTrimeshOutputScaleControlEdit(activeParameterId, value);
            }
            break;

        case TrimeshExpandedHitRegionKind::VertexParameter:
            dragTarget = DragTarget::VertexParameter;
            activeParameterId = region.parameterId;

            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
                delegate->beginTrimeshVertexControlEdit(activeParameterId, value);
            }
            break;

        case TrimeshExpandedHitRegionKind::VertexGuideGain:
            if (!widget.hasGuideAttachmentForParameter(region.parameterId)
                    || !widget.guideGainValueForParameter(
                            region.parameterId,
                            guideGainDragStartValue)) {
                break;
            }
            dragTarget = DragTarget::VertexGuideGain;
            activeParameterId = region.parameterId;
            guideGainDragStartY = position.y;
            if (delegate != nullptr) {
                delegate->beginTrimeshVertexControlEdit(
                        activeParameterId,
                        guideGainDragStartValue);
            }
            break;

        case TrimeshExpandedHitRegionKind::VertexGuideAttachment:
            if (delegate != nullptr) {
                delegate->showTrimeshVertexGuideMenu(region.parameterId, screenArea);
            }
            break;
    }
}

void TrimeshControlsComponent::dragControl(Point<float> position) {
    float value {};

    switch (dragTarget) {
        case DragTarget::Morph:
            if (widget.morphValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
                delegate->updateTrimeshMorphControlEdit(value);
            }
            break;

        case DragTarget::OutputScale:
            if (widget.outputScaleValueAt(contentBounds, position, value)
                    && delegate != nullptr) {
                delegate->updateTrimeshOutputScaleControlEdit(value);
            }
            break;

        case DragTarget::VertexParameter:
            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
                delegate->updateTrimeshVertexControlEdit(value);
            }
            break;

        case DragTarget::VertexGuideGain:
            value = jlimit(
                    0.f,
                    1.f,
                    guideGainDragStartValue
                            + (guideGainDragStartY - position.y) / 120.f);
            if (delegate != nullptr) {
                delegate->updateTrimeshVertexControlEdit(value);
            }
            break;

        case DragTarget::None:
            break;
    }
}

void TrimeshControlsComponent::endControlDrag() {
    if (dragTarget == DragTarget::Morph && delegate != nullptr) {
        delegate->endTrimeshMorphControlEdit();
    } else if (dragTarget == DragTarget::OutputScale && delegate != nullptr) {
        delegate->endTrimeshOutputScaleControlEdit();
    } else if ((dragTarget == DragTarget::VertexParameter
            || dragTarget == DragTarget::VertexGuideGain) && delegate != nullptr) {
        delegate->endTrimeshVertexControlEdit();
    }

    dragTarget = DragTarget::None;
    activeParameterId = {};
}

bool TrimeshControlsComponent::adjustGuideGainFromKeyboard(
        const TrimeshExpandedHitRegion& region,
        float amount) {
    float value {};
    if (delegate == nullptr
            || !widget.hasGuideAttachmentForParameter(region.parameterId)
            || !widget.guideGainValueForParameter(region.parameterId, value)) {
        return false;
    }

    delegate->beginTrimeshVertexControlEdit(region.parameterId, value);
    delegate->updateTrimeshVertexControlEdit(jlimit(0.f, 1.f, value + amount));
    delegate->endTrimeshVertexControlEdit();
    return true;
}

void TrimeshControlsComponent::rememberMorphFocus(
        const TrimeshExpandedHitRegion& region) {
    String axis;
    if (region.kind == TrimeshExpandedHitRegionKind::MorphControl) {
        axis = region.parameterId;
    } else if (region.kind == TrimeshExpandedHitRegionKind::PrimaryAxis
            || region.kind == TrimeshExpandedHitRegionKind::LinkToggle) {
        axis = region.axisValue;
    }
    if (axis.isEmpty() || axis == keyboardMorphAxis) {
        return;
    }
    keyboardMorphAxis = axis;
    updateKeyboardShortcutHint();
    repaint();
}

bool TrimeshControlsComponent::selectMorphControl(const String& axis) {
    for (size_t index = 0; index < controlRegions.size(); ++index) {
        const auto& region = controlHitRegions[index];
        if (region.kind != TrimeshExpandedHitRegionKind::MorphControl
                || region.parameterId != axis) {
            continue;
        }
        keyboardMorphAxis = axis;
        updateKeyboardShortcutHint();
        repaint();
        return true;
    }
    return false;
}

bool TrimeshControlsComponent::selectAdjacentMorphControl(int direction) {
    const std::array<String, 3> axes {
            String("yellow"),
            String("red"),
            String("blue")
    };
    const auto current = std::find(axes.begin(), axes.end(), focusedMorphAxis());
    const int currentIndex = current == axes.end()
            ? (direction < 0 ? static_cast<int>(axes.size()) : -1)
            : static_cast<int>(std::distance(axes.begin(), current));
    for (int index = currentIndex + direction; index >= 0 && index < 3; index += direction) {
        if (selectMorphControl(axes[(size_t) index])) {
            return true;
        }
    }
    return current != axes.end();
}

bool TrimeshControlsComponent::setFocusedMorphTo(float value) {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty() || delegate == nullptr) {
        return false;
    }
    delegate->beginTrimeshMorphControlEdit(axis, value);
    delegate->endTrimeshMorphControlEdit();
    return true;
}

bool TrimeshControlsComponent::adjustFocusedMorphBy(float amount) {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty() || delegate == nullptr) {
        return false;
    }
    const float value = delegate->trimeshMorphControlValue(axis);
    return setFocusedMorphTo(jlimit(0.f, 1.f, value + amount));
}

bool TrimeshControlsComponent::toggleFocusedMorphLink() {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty() || delegate == nullptr) {
        return false;
    }
    delegate->toggleTrimeshLinkAxis(axis);
    return true;
}

void TrimeshControlsComponent::updateKeyboardShortcutHint() {
    const String axis = focusedMorphAxis();
    if (axis.isEmpty()
            || node.kind != NodeKind::TrilinearMesh
            || contentBounds.isEmpty()) {
        keyboardShortcutHelp->hide();
        return;
    }

    const int axisIndex = axis == "yellow" ? 0 : axis == "red" ? 1 : 2;
    const Rectangle<float> focus = TrimeshWidget::expandedMorphKeyboardFocusBounds(
            contentBounds,
            axisIndex,
            widget.showsOutputScale(),
            MeshDimensionality::dimensions(node).axisCount());
    keyboardShortcutHelp->setAnchorBounds(focus);
}

}
