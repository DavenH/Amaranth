#include "TrimeshControlsComponent.h"

#include <algorithm>
#include <utility>

namespace CycleV2 {

class TrimeshControlsComponent::ControlTarget : public Component {
public:
    ControlTarget(
            TrimeshControlsComponent& targetOwner,
            TrimeshExpandedHitRegion targetRegion) :
            owner   (targetOwner)
        ,   region  (std::move(targetRegion)) {
        setOpaque(false);
        setMouseCursor(
                region.kind == TrimeshExpandedHitRegionKind::MorphControl
                        || region.kind == TrimeshExpandedHitRegionKind::VertexParameter
                ? MouseCursor::LeftRightResizeCursor
                : MouseCursor::PointingHandCursor);
    }

    void paint(Graphics&) override {
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
}

TrimeshControlsComponent::~TrimeshControlsComponent() = default;

void TrimeshControlsComponent::setDelegate(TrimeshControlsDelegate* nextDelegate) {
    delegate = nextDelegate;
}

void TrimeshControlsComponent::setNode(const Node& nextNode) {
    node = nextNode;
    updateHitRegions();
}

void TrimeshControlsComponent::setContentBounds(Rectangle<float> nextContentBounds) {
    contentBounds = nextContentBounds;
    updateHitRegions();
}

int TrimeshControlsComponent::getPrimaryAxisButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::PrimaryAxis);
}

int TrimeshControlsComponent::getMorphSliderCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::MorphControl);
}

int TrimeshControlsComponent::getLinkToggleButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::LinkToggle);
}

int TrimeshControlsComponent::getVertexParameterSliderCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::VertexParameter);
}

int TrimeshControlsComponent::getVertexGuideAttachmentButtonCount() const {
    return countControlRegions(TrimeshExpandedHitRegionKind::VertexGuideAttachment);
}

void TrimeshControlsComponent::resized() {
    updateHitRegions();
}

MouseCursor TrimeshControlsComponent::cursorFor(Point<float> position) {
    const auto* region = findControlRegion(position);

    if (region != nullptr) {
        if (region->kind == TrimeshExpandedHitRegionKind::MorphControl
                || region->kind == TrimeshExpandedHitRegionKind::VertexParameter) {
            return MouseCursor::LeftRightResizeCursor;
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
        return;
    }

    for (auto& region : controlRegions) {
        removeChildComponent(region.get());
    }

    controlRegions.clear();
    controlHitRegions.clear();
    lastHitRegionContentBounds = nextContentBounds;

    if (node.kind != NodeKind::TrilinearMesh || contentBounds.isEmpty()) {
        return;
    }

    controlHitRegions = widget.expandedControlHitRegions(contentBounds);

    for (const auto& region : controlHitRegions) {
        auto component = std::make_unique<ControlTarget>(*this, region);
        component->setBounds(region.bounds.toNearestInt());
        addAndMakeVisible(component.get());
        controlRegions.push_back(std::move(component));
    }
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
            [position](const TrimeshExpandedHitRegion& region) {
                return region.bounds.contains(position);
            });

    if (found == controlHitRegions.end()) {
        return nullptr;
    }

    return &*found;
}

void TrimeshControlsComponent::beginControlDrag(
        const TrimeshExpandedHitRegion& region,
        Point<float> position,
        Rectangle<int> screenArea) {
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

        case TrimeshExpandedHitRegionKind::VertexParameter:
            dragTarget = DragTarget::VertexParameter;
            activeParameterId = region.parameterId;

            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
                delegate->beginTrimeshVertexControlEdit(activeParameterId, value);
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

        case DragTarget::VertexParameter:
            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && delegate != nullptr) {
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
    } else if (dragTarget == DragTarget::VertexParameter && delegate != nullptr) {
        delegate->endTrimeshVertexControlEdit();
    }

    dragTarget = DragTarget::None;
    activeParameterId = {};
}

}
