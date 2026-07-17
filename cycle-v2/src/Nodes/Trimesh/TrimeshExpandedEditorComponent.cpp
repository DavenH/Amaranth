#include "TrimeshExpandedEditorComponent.h"

#include <utility>

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kHeaderHeight = 34.f;

}

TrimeshExpandedEditorComponent::TrimeshExpandedEditorComponent(TrimeshWidget& targetWidget) :
        widget      (targetWidget)
    ,   controls    (targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(controls);
}

TrimeshExpandedEditorComponent::~TrimeshExpandedEditorComponent() = default;

void TrimeshExpandedEditorComponent::setDelegate(TrimeshExpandedEditorDelegate* nextDelegate) {
    delegate = nextDelegate;
    controls.setDelegate(this);

    auto safeThis = Component::SafePointer<TrimeshExpandedEditorComponent>(this);
    widget.setExpandedPanelCallbacks(
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->repaint();

                    if (safeThis->delegate != nullptr) {
                        safeThis->delegate->repaintTrimeshEditorOpenGL();
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
    updateControlsHost();
    repaint();
}

void TrimeshExpandedEditorComponent::setGuideAttachmentLabels(std::array<String, 6> labels) {
    widget.setGuideAttachmentLabels(std::move(labels));
    repaint();
}

void TrimeshExpandedEditorComponent::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshExpandedEditorComponent::setRenderProfile(TrimeshRenderProfile profile) {
    renderProfile = profile;
    widget.setRenderProfile(profile);
    repaint();
}

void TrimeshExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    if (node.kind != NodeKind::TrilinearMesh) {
        return;
    }

    widget.setRenderProfile(renderProfile);
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

    widget.setRenderProfile(renderProfile);
    widget.paintExpanded(g, node, content);

    panel = getLocalBounds().toFloat();
    g.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
    g.drawRoundedRectangle(panel.reduced(0.75f), 8.f, 1.3f);
}

void TrimeshExpandedEditorComponent::resized() {
    updatePanelHosts();
    updateControlsHost();
}

void TrimeshExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    updateCursor(event.position);
}

void TrimeshExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    if (closeButtonBounds().contains(event.position)) {
        if (delegate != nullptr) {
            delegate->closeTrimeshEditor();
        }
        return;
    }

    controls.beginPointerInteraction(
            event.position,
            Rectangle<int>(localPointToGlobal(event.position.roundToInt()), { 1, 1 }));
}

void TrimeshExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    controls.continuePointerInteraction(event.position);
}

void TrimeshExpandedEditorComponent::mouseUp(const MouseEvent&) {
    controls.endPointerInteraction();
}

void TrimeshExpandedEditorComponent::setTrimeshPrimaryAxis(const String& axis) {
    if (delegate != nullptr) {
        delegate->setTrimeshPrimaryAxisValue(axis);
    }
}

void TrimeshExpandedEditorComponent::toggleTrimeshLinkAxis(const String& axis) {
    if (delegate != nullptr) {
        delegate->toggleTrimeshLinkAxisValue(axis);
    }
}

void TrimeshExpandedEditorComponent::beginTrimeshMorphControlEdit(
        const String& id,
        float value) {
    if (delegate != nullptr) {
        delegate->beginTrimeshMorphEdit(id, value);
    }
}

void TrimeshExpandedEditorComponent::updateTrimeshMorphControlEdit(float value) {
    if (delegate != nullptr) {
        delegate->updateTrimeshMorphEdit(value);
    }
}

void TrimeshExpandedEditorComponent::endTrimeshMorphControlEdit() {
    if (delegate != nullptr) {
        delegate->endTrimeshMorphEdit();
    }
}

void TrimeshExpandedEditorComponent::beginTrimeshVertexControlEdit(
        const String& id,
        float value) {
    if (delegate != nullptr) {
        delegate->beginTrimeshVertexParameterEdit(id, value);
    }
}

void TrimeshExpandedEditorComponent::updateTrimeshVertexControlEdit(float value) {
    if (delegate != nullptr) {
        delegate->updateTrimeshVertexParameterEdit(value);
    }
}

void TrimeshExpandedEditorComponent::endTrimeshVertexControlEdit() {
    if (delegate != nullptr) {
        delegate->endTrimeshVertexParameterEdit();
    }
}

void TrimeshExpandedEditorComponent::showTrimeshVertexGuideMenu(
        const String& id,
        Rectangle<int> screenArea) {
    if (delegate != nullptr) {
        delegate->showTrimeshGuideAttachmentMenu(vertexGuideParameterField(id), screenArea);
    }
}

void TrimeshExpandedEditorComponent::selectTrimeshVertex(int index) {
    if (delegate != nullptr) {
        delegate->selectTrimeshVertex(index);
    }
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

String TrimeshExpandedEditorComponent::vertexGuideParameterField(const String& parameterId) const {
    return parameterId.fromLastOccurrenceOf(".", false, false);
}

MouseCursor TrimeshExpandedEditorComponent::cursorFor(Point<float> position) {
    if (closeButtonBounds().contains(position)) {
        return MouseCursor::PointingHandCursor;
    }

    return controls.cursorFor(position);
}

void TrimeshExpandedEditorComponent::updateCursor(Point<float> position) {
    setMouseCursor(cursorFor(position));
}

void TrimeshExpandedEditorComponent::updatePanelHosts() {
    if (node.kind != NodeKind::TrilinearMesh || getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    const Rectangle<float> content = contentBounds();
    widget.setRenderProfile(renderProfile);
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

    controls.toFront(false);
}

void TrimeshExpandedEditorComponent::updateControlsHost() {
    controls.setBounds(getLocalBounds());
    controls.setNode(node);
    controls.setContentBounds(contentBounds());
    controls.setVisible(node.kind == NodeKind::TrilinearMesh);
    controls.toFront(false);
}

}
