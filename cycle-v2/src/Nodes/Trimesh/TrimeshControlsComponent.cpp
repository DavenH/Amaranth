#include "TrimeshControlsComponent.h"

#include <utility>

namespace CycleV2 {

class TrimeshControlsComponent::ControlSlider : public Slider {
public:
    ControlSlider(
            TrimeshControlsComponent& owner,
            TrimeshExpandedHitRegion region) :
            Slider(region.parameterId)
        ,   owner(owner)
        ,   region(std::move(region)) {
        setRange(0.0, 1.0, 0.0);
        setSliderStyle(Slider::LinearHorizontal);
        setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        setScrollWheelEnabled(false);
        setOpaque(false);
        setInterceptsMouseClicks(true, true);
        setMouseCursor(MouseCursor::LeftRightResizeCursor);
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

    TrimeshExpandedHitRegionKind getRegionKind() const { return region.kind; }

private:
    Point<float> parentPosition(Point<float> localPosition) const {
        return getBounds().toFloat().getTopLeft() + localPosition;
    }

    TrimeshControlsComponent& owner;
    TrimeshExpandedHitRegion region;
};

class TrimeshControlsComponent::PrimaryAxisButton : public Button {
public:
    PrimaryAxisButton(
            TrimeshControlsComponent& owner,
            TrimeshExpandedHitRegion region) :
            Button(region.axisValue)
        ,   owner(owner)
        ,   region(std::move(region)) {
        setOpaque(false);
        setMouseCursor(MouseCursor::PointingHandCursor);
        setTriggeredOnMouseDown(true);
    }

    void paintButton(Graphics&, bool, bool) override {}

    void clicked() override {
        owner.beginControlDrag(region, getBounds().toFloat().getCentre());
    }

private:
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

int TrimeshControlsComponent::getPrimaryAxisButtonCount() const {
    int count {};

    for (const auto& region : controlRegions) {
        if (dynamic_cast<PrimaryAxisButton*>(region.get()) != nullptr) {
            ++count;
        }
    }

    return count;
}

int TrimeshControlsComponent::getMorphSliderCount() const {
    int count {};

    for (const auto& region : controlRegions) {
        const auto* slider = dynamic_cast<ControlSlider*>(region.get());

        if (slider != nullptr && slider->getRegionKind() == TrimeshExpandedHitRegionKind::MorphControl) {
            ++count;
        }
    }

    return count;
}

int TrimeshControlsComponent::getVertexParameterSliderCount() const {
    int count {};

    for (const auto& region : controlRegions) {
        const auto* slider = dynamic_cast<ControlSlider*>(region.get());

        if (slider != nullptr && slider->getRegionKind() == TrimeshExpandedHitRegionKind::VertexParameter) {
            ++count;
        }
    }

    return count;
}

void TrimeshControlsComponent::resized() {
    updateHitRegions();
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
    lastHitRegionContentBounds = nextContentBounds;

    if (node.kind != NodeKind::TrilinearMesh || contentBounds.isEmpty()) {
        return;
    }

    for (const auto& region : widget.expandedControlHitRegions(contentBounds)) {
        std::unique_ptr<Component> component;

        if (region.kind == TrimeshExpandedHitRegionKind::PrimaryAxis) {
            component = std::make_unique<PrimaryAxisButton>(*this, region);
        } else {
            component = std::make_unique<ControlSlider>(*this, region);
        }

        component->setBounds(region.bounds.toNearestInt());
        addAndMakeVisible(component.get());
        controlRegions.push_back(std::move(component));
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
