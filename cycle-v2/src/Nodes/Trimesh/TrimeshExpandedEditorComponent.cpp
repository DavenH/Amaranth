#include "TrimeshExpandedEditorComponent.h"

#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kHeaderHeight = 34.f;

}

class TrimeshExpandedEditorComponent::HitRegionComponent : public Component {
public:
    HitRegionComponent(
            TrimeshExpandedEditorComponent& owner,
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

    TrimeshExpandedEditorComponent& owner;
    TrimeshExpandedHitRegion region;
};

TrimeshExpandedEditorComponent::TrimeshExpandedEditorComponent(TrimeshWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
}

TrimeshExpandedEditorComponent::~TrimeshExpandedEditorComponent() = default;

void TrimeshExpandedEditorComponent::setCallbacks(Callbacks nextCallbacks) {
    callbacks = std::move(nextCallbacks);

    auto safeThis = Component::SafePointer<TrimeshExpandedEditorComponent>(this);
    widget.setExpandedPanelCallbacks(
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->repaint();

                    if (safeThis->callbacks.repaintOpenGL != nullptr) {
                        safeThis->callbacks.repaintOpenGL();
                    }
                }
            },
            [safeThis](const MouseCursor& cursor) {
                if (safeThis != nullptr) {
                    safeThis->setMouseCursor(cursor);
                }
            },
            [safeThis](Point<float> screenPosition) {
                if (safeThis != nullptr) {
                    safeThis->updateCursor(
                            safeThis->getLocalPoint(nullptr, screenPosition.roundToInt()).toFloat());
                }
            });
}

void TrimeshExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    updatePanelHosts();
    updateHitRegions();
    repaint();
}

void TrimeshExpandedEditorComponent::setDisplayDomain(PortDomain domain) {
    displayDomain = domain;
    widget.setDisplayDomain(domain);
    repaint();
}

void TrimeshExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    if (node.kind != NodeKind::TrilinearMesh) {
        return;
    }

    widget.setDisplayDomain(displayDomain);
    widget.renderExpandedPanelsOpenGL(
            node,
            contentBounds().translated((float) getX(), (float) getY()),
            scaleFactor);
}

void TrimeshExpandedEditorComponent::paint(Graphics& g) {
    Rectangle<float> panel = getLocalBounds().toFloat();
    const Rectangle<float> content = contentBounds();
    const Rectangle<int> gridHole = TrimeshWidget::expandedGridPanelContentBounds(content).toNearestInt();
    const Rectangle<int> waveHole = TrimeshWidget::expandedWavePanelContentBounds(content).toNearestInt();

    g.saveState();
    g.excludeClipRegion(gridHole);
    g.excludeClipRegion(waveHole);
    g.setColour(Colours::black.withAlpha(0.38f));
    g.fillRoundedRectangle(panel.translated(0.f, 10.f), 8.f);
    g.setColour(Colour(0xff141a21));
    g.fillRoundedRectangle(panel, 8.f);
    g.restoreState();

    g.setColour(Colour(0xff17d0c5).withAlpha(0.72f));
    g.drawRoundedRectangle(panel, 8.f, 1.5f);

    auto header = panel.removeFromTop(kHeaderHeight);
    g.setColour(Colour(0xff202833));
    g.fillRoundedRectangle(header, 8.f);
    g.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));

    g.setColour(kText);
    g.setFont(FontOptions(14.f, Font::bold));
    g.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);
    g.setColour(kMutedText);
    g.setFont(FontOptions(10.f));
    g.drawText("Trilinear Mesh", header.reduced(13.f, 4.f), Justification::centredRight);

    Rectangle<float> closeButton = closeButtonBounds();
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050));
    g.drawEllipse(closeButton, 1.f);
    g.setColour(kText);
    g.drawLine(closeButton.getX() + 7.f, closeButton.getY() + 7.f,
               closeButton.getRight() - 7.f, closeButton.getBottom() - 7.f, 1.4f);
    g.drawLine(closeButton.getRight() - 7.f, closeButton.getY() + 7.f,
               closeButton.getX() + 7.f, closeButton.getBottom() - 7.f, 1.4f);

    widget.setDisplayDomain(displayDomain);
    widget.paintExpanded(g, node, content);
}

void TrimeshExpandedEditorComponent::resized() {
    updatePanelHosts();
    updateHitRegions();
}

void TrimeshExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    updateCursor(event.position);
}

void TrimeshExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    if (closeButtonBounds().contains(event.position)) {
        if (callbacks.close != nullptr) {
            callbacks.close();
        }
        return;
    }

    const Rectangle<float> content = contentBounds();
    String parameterId;
    String axisValue;
    float value {};
    int vertexIndex {};

    if (widget.findPrimaryAxisAt(content, event.position, axisValue)) {
        if (callbacks.setPrimaryAxis != nullptr) {
            callbacks.setPrimaryAxis(axisValue);
        }
        return;
    }

    if (widget.findMorphControlAt(content, event.position, parameterId, value)) {
        dragTarget = DragTarget::Morph;
        activeParameterId = parameterId;

        if (callbacks.beginMorphEdit != nullptr) {
            callbacks.beginMorphEdit(parameterId, value);
        }
        return;
    }

    if (widget.findVertexParameterAt(content, event.position, parameterId, value)) {
        dragTarget = DragTarget::VertexParameter;
        activeParameterId = parameterId;

        if (callbacks.beginVertexParameterEdit != nullptr) {
            callbacks.beginVertexParameterEdit(parameterId, value);
        }
        return;
    }

    if (widget.findVertexSelectionAt(node, content, event.position, vertexIndex)) {
        if (callbacks.selectVertex != nullptr) {
            callbacks.selectVertex(vertexIndex);
        }
    }
}

void TrimeshExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    const Rectangle<float> content = contentBounds();
    float value {};

    switch (dragTarget) {
        case DragTarget::Morph:
            if (widget.morphValueForParameterAt(content, activeParameterId, event.position, value)
                    && callbacks.updateMorphEdit != nullptr) {
                callbacks.updateMorphEdit(value);
            }
            break;

        case DragTarget::VertexParameter:
            if (widget.vertexParameterValueForParameterAt(content, activeParameterId, event.position, value)
                    && callbacks.updateVertexParameterEdit != nullptr) {
                callbacks.updateVertexParameterEdit(value);
            }
            break;

        case DragTarget::None:
            break;
    }
}

void TrimeshExpandedEditorComponent::mouseUp(const MouseEvent&) {
    if (dragTarget == DragTarget::Morph && callbacks.endMorphEdit != nullptr) {
        callbacks.endMorphEdit();
    } else if (dragTarget == DragTarget::VertexParameter && callbacks.endVertexParameterEdit != nullptr) {
        callbacks.endVertexParameterEdit();
    }

    dragTarget = DragTarget::None;
    activeParameterId = {};
}

Rectangle<float> TrimeshExpandedEditorComponent::closeButtonBounds() const {
    const Rectangle<float> panel = getLocalBounds().toFloat();
    return Rectangle<float>(22.f, 22.f).withCentre({ panel.getRight() - 22.f, kHeaderHeight * 0.5f });
}

Rectangle<float> TrimeshExpandedEditorComponent::contentBounds() const {
    Rectangle<float> panel = getLocalBounds().toFloat();
    panel.removeFromTop(kHeaderHeight);
    return panel.reduced(10.f, 8.f);
}

MouseCursor TrimeshExpandedEditorComponent::cursorFor(Point<float> position) {
    if (closeButtonBounds().contains(position)) {
        return MouseCursor::PointingHandCursor;
    }

    const Rectangle<float> content = contentBounds();
    String parameterId;
    String axisValue;
    float value {};
    int vertexIndex {};

    if (widget.findPrimaryAxisAt(content, position, axisValue)
            || widget.findVertexSelectionAt(node, content, position, vertexIndex)) {
        return MouseCursor::PointingHandCursor;
    }

    if (widget.findMorphControlAt(content, position, parameterId, value)
            || widget.findVertexParameterAt(content, position, parameterId, value)) {
        return MouseCursor::LeftRightResizeCursor;
    }

    return MouseCursor::NormalCursor;
}

void TrimeshExpandedEditorComponent::updateCursor(Point<float> position) {
    setMouseCursor(cursorFor(position));
}

void TrimeshExpandedEditorComponent::updatePanelHosts() {
    if (node.kind != NodeKind::TrilinearMesh || getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    const Rectangle<float> content = contentBounds();
    widget.setDisplayDomain(displayDomain);
    Component* panel3D = widget.getExpandedPanel3DComponentIfCreated();
    Component* panel2D = widget.getExpandedPanel2DComponentIfCreated();

    if (panel3D == nullptr) {
        panel3D = widget.prepareExpandedPanel3DComponent(node, content);
    }

    if (panel2D == nullptr) {
        panel2D = widget.prepareExpandedPanel2DComponent(node, content);
    }

    if (panel3D == nullptr || panel2D == nullptr) {
        return;
    }

    const Rectangle<int> panel3DBounds = TrimeshWidget::expandedGridPanelContentBounds(content).toNearestInt();
    const Rectangle<int> panel2DBounds = TrimeshWidget::expandedWavePanelContentBounds(content).toNearestInt();

    if (panel3D->getParentComponent() != this) {
        addAndMakeVisible(panel3D);
    }

    if (panel3D->getBounds() != panel3DBounds) {
        panel3D->setBounds(panel3DBounds);
    }

    panel3D->setVisible(true);
    panel3D->toFront(false);

    if (panel2D->getParentComponent() != this) {
        addAndMakeVisible(panel2D);
    }

    if (panel2D->getBounds() != panel2DBounds) {
        panel2D->setBounds(panel2DBounds);
    }

    panel2D->setVisible(true);
    panel2D->toFront(false);
}

void TrimeshExpandedEditorComponent::updateHitRegions() {
    const Rectangle<int> nextContentBounds = contentBounds().toNearestInt();

    if (nextContentBounds == lastHitRegionContentBounds && !hitRegions.empty()) {
        return;
    }

    for (auto& region : hitRegions) {
        removeChildComponent(region.get());
    }

    hitRegions.clear();
    lastHitRegionContentBounds = nextContentBounds;

    if (node.kind != NodeKind::TrilinearMesh || getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    for (const auto& region : widget.expandedControlHitRegions(contentBounds())) {
        auto component = std::make_unique<HitRegionComponent>(*this, region);
        component->setBounds(region.bounds.toNearestInt());
        addAndMakeVisible(component.get());
        component->toFront(false);
        hitRegions.push_back(std::move(component));
    }
}

void TrimeshExpandedEditorComponent::beginControlDrag(
        const TrimeshExpandedHitRegion& region,
        Point<float> position) {
    const Rectangle<float> content = contentBounds();
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

            if (widget.morphValueForParameterAt(content, activeParameterId, position, value)
                    && callbacks.beginMorphEdit != nullptr) {
                callbacks.beginMorphEdit(activeParameterId, value);
            }
            break;

        case TrimeshExpandedHitRegionKind::VertexParameter:
            dragTarget = DragTarget::VertexParameter;
            activeParameterId = region.parameterId;

            if (widget.vertexParameterValueForParameterAt(content, activeParameterId, position, value)
                    && callbacks.beginVertexParameterEdit != nullptr) {
                callbacks.beginVertexParameterEdit(activeParameterId, value);
            }
            break;
    }
}

void TrimeshExpandedEditorComponent::dragControl(
        const TrimeshExpandedHitRegion& region,
        Point<float> position) {
    ignoreUnused(region);
    const Rectangle<float> content = contentBounds();
    float value {};

    switch (dragTarget) {
        case DragTarget::Morph:
            if (widget.morphValueForParameterAt(content, activeParameterId, position, value)
                    && callbacks.updateMorphEdit != nullptr) {
                callbacks.updateMorphEdit(value);
            }
            break;

        case DragTarget::VertexParameter:
            if (widget.vertexParameterValueForParameterAt(content, activeParameterId, position, value)
                    && callbacks.updateVertexParameterEdit != nullptr) {
                callbacks.updateVertexParameterEdit(value);
            }
            break;

        case DragTarget::None:
            break;
    }
}

void TrimeshExpandedEditorComponent::endControlDrag() {
    if (dragTarget == DragTarget::Morph && callbacks.endMorphEdit != nullptr) {
        callbacks.endMorphEdit();
    } else if (dragTarget == DragTarget::VertexParameter && callbacks.endVertexParameterEdit != nullptr) {
        callbacks.endVertexParameterEdit();
    }

    dragTarget = DragTarget::None;
    activeParameterId = {};
}

}
