#include "TrimeshControlsComponent.h"

#include <utility>

namespace CycleV2 {

class TrimeshControlsComponent::HitRegionComponent : public Component {
public:
    HitRegionComponent(
            TrimeshControlsComponent& owner,
            TrimeshExpandedHitRegion region) :
            owner(owner)
        ,   region(std::move(region)) {
        setOpaque(false);
        setInterceptsMouseClicks(true, true);
        setMouseCursor(cursorForRegion());
    }

    void paint(Graphics&) override {}

    void mouseDown(const MouseEvent& event) override {
        owner.beginControlDrag(region, parentPosition(event.position));
    }

    void mouseDrag(const MouseEvent& event) override {
        owner.dragControl(region, parentPosition(event.position));
    }

    void mouseUp(const MouseEvent&) override {
        owner.endControlDrag();
    }

private:
    MouseCursor cursorForRegion() const {
        switch (region.kind) {
            case TrimeshExpandedHitRegionKind::MorphControl:
            case TrimeshExpandedHitRegionKind::VertexParameter:
                return MouseCursor::LeftRightResizeCursor;

            case TrimeshExpandedHitRegionKind::PrimaryAxis:
                return MouseCursor::PointingHandCursor;
        }

        return MouseCursor::NormalCursor;
    }

    Point<float> parentPosition(Point<float> localPosition) const {
        return getBounds().toFloat().getTopLeft() + localPosition;
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

void TrimeshControlsComponent::setCallbacks(Callbacks nextCallbacks) {
    callbacks = std::move(nextCallbacks);
}

void TrimeshControlsComponent::setNode(const Node& nextNode) {
    node = nextNode;
    updateHitRegions();
}

void TrimeshControlsComponent::setContentBounds(Rectangle<float> nextContentBounds) {
    contentBounds = nextContentBounds;
    updateHitRegions();
}

void TrimeshControlsComponent::resized() {
    updateHitRegions();
}

void TrimeshControlsComponent::updateHitRegions() {
    const Rectangle<int> nextContentBounds = contentBounds.toNearestInt();

    if (nextContentBounds == lastHitRegionContentBounds && !hitRegions.empty()) {
        return;
    }

    for (auto& region : hitRegions) {
        removeChildComponent(region.get());
    }

    hitRegions.clear();
    lastHitRegionContentBounds = nextContentBounds;

    if (node.kind != NodeKind::TrilinearMesh || contentBounds.isEmpty()) {
        return;
    }

    for (const auto& region : widget.expandedControlHitRegions(contentBounds)) {
        auto component = std::make_unique<HitRegionComponent>(*this, region);
        component->setBounds(region.bounds.toNearestInt());
        addAndMakeVisible(component.get());
        hitRegions.push_back(std::move(component));
    }
}

void TrimeshControlsComponent::beginControlDrag(
        const TrimeshExpandedHitRegion& region,
        Point<float> position) {
    float value {};

    switch (region.kind) {
        case TrimeshExpandedHitRegionKind::PrimaryAxis:
            if (callbacks.setPrimaryAxis != nullptr) {
                callbacks.setPrimaryAxis(region.axisValue);
            }
            break;

        case TrimeshExpandedHitRegionKind::MorphControl:
            dragTarget = DragTarget::Morph;
            activeParameterId = region.parameterId;

            if (widget.morphValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && callbacks.beginMorphEdit != nullptr) {
                callbacks.beginMorphEdit(activeParameterId, value);
            }
            break;

        case TrimeshExpandedHitRegionKind::VertexParameter:
            dragTarget = DragTarget::VertexParameter;
            activeParameterId = region.parameterId;

            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && callbacks.beginVertexParameterEdit != nullptr) {
                callbacks.beginVertexParameterEdit(activeParameterId, value);
            }
            break;
    }
}

void TrimeshControlsComponent::dragControl(
        const TrimeshExpandedHitRegion&,
        Point<float> position) {
    float value {};

    switch (dragTarget) {
        case DragTarget::Morph:
            if (widget.morphValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && callbacks.updateMorphEdit != nullptr) {
                callbacks.updateMorphEdit(value);
            }
            break;

        case DragTarget::VertexParameter:
            if (widget.vertexParameterValueForParameterAt(contentBounds, activeParameterId, position, value)
                    && callbacks.updateVertexParameterEdit != nullptr) {
                callbacks.updateVertexParameterEdit(value);
            }
            break;

        case DragTarget::None:
            break;
    }
}

void TrimeshControlsComponent::endControlDrag() {
    if (dragTarget == DragTarget::Morph && callbacks.endMorphEdit != nullptr) {
        callbacks.endMorphEdit();
    } else if (dragTarget == DragTarget::VertexParameter && callbacks.endVertexParameterEdit != nullptr) {
        callbacks.endVertexParameterEdit();
    }

    dragTarget = DragTarget::None;
    activeParameterId = {};
}

}
