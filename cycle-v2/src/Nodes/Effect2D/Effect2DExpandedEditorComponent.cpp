#include "Effect2DExpandedEditorComponent.h"

#include "CurveNodeModels.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
constexpr float kHeaderHeight = 34.f;

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return object;
}

}

Effect2DExpandedEditorComponent::Effect2DExpandedEditorComponent(Effect2DWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
}

Effect2DExpandedEditorComponent::~Effect2DExpandedEditorComponent() = default;

void Effect2DExpandedEditorComponent::setCallbacks(Callbacks nextCallbacks) {
    callbacks = std::move(nextCallbacks);
    auto safeThis = Component::SafePointer<Effect2DExpandedEditorComponent>(this);
    widget.setExpandedPanelCallbacks(
            [safeThis] {
                if (safeThis != nullptr) {
                    safeThis->requestRepaint();
                }
            },
            [safeThis](const MouseCursor& cursor) {
                if (safeThis != nullptr) {
                    safeThis->setMouseCursor(cursor);
                }
            });
    widget.setMeshEditedCallback([safeThis] {
        if (safeThis != nullptr) {
            safeThis->persistEffectMeshState();
        }
    });
}

void Effect2DExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    const ScopedValueSetter<bool> guard(syncingControls, true);
    syncEditorFromNode();
    applyEditorStateToWidget();
    updatePanelHost();
    layoutEditor();
    repaint();
}

void Effect2DExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    widget.renderExpandedPanelOpenGL(
            node,
            editorPanelBounds().translated((float) getX(), (float) getY()),
            getLocalBounds().toFloat().translated((float) getX(), (float) getY()),
            scaleFactor);
}

void Effect2DExpandedEditorComponent::paint(Graphics& graphics) {
    Rectangle<float> outer = getLocalBounds().toFloat();
    graphics.saveState();
    graphics.excludeClipRegion(editorPanelBounds().toNearestInt());
    graphics.setColour(Colours::black.withAlpha(0.38f));
    graphics.fillRoundedRectangle(outer.translated(0.f, 10.f), 8.f);
    graphics.setColour(Colour(0xff141a21));
    graphics.fillRoundedRectangle(outer, 8.f);
    graphics.restoreState();

    auto header = outer.removeFromTop(kHeaderHeight);
    graphics.setColour(Colour(0xff202833));
    graphics.fillRoundedRectangle(header, 8.f);
    graphics.fillRect(header.withTrimmedTop(header.getHeight() - 8.f));
    graphics.setColour(kText);
    graphics.setFont(FontOptions(14.f, Font::bold));
    graphics.drawText(node.title, header.reduced(13.f, 4.f), Justification::centredLeft);

    paintEditor(graphics);

    const Rectangle<float> close = closeButtonBounds();
    graphics.setColour(Colour(0xff0e1318));
    graphics.fillEllipse(close);
    graphics.setColour(Colour(0xff354050));
    graphics.drawEllipse(close, 1.f);
    graphics.setColour(kText);
    graphics.drawLine(close.getX() + 7.f, close.getY() + 7.f,
            close.getRight() - 7.f, close.getBottom() - 7.f, 1.4f);
    graphics.drawLine(close.getRight() - 7.f, close.getY() + 7.f,
            close.getX() + 7.f, close.getBottom() - 7.f, 1.4f);
    graphics.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
    graphics.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.75f), 8.f, 1.3f);
}

void Effect2DExpandedEditorComponent::resized() {
    updatePanelHost();
    layoutEditor();
}

void Effect2DExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    if (!editorMouseMove(event.position)) {
        setMouseCursor(closeButtonBounds().contains(event.position)
                ? MouseCursor::PointingHandCursor
                : MouseCursor::NormalCursor);
    }
}

void Effect2DExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    if (closeButtonBounds().contains(event.position)) {
        if (callbacks.close != nullptr) {
            callbacks.close();
        }
        return;
    }
    beginTransaction();
    editorMouseDown(event.position);
}

void Effect2DExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    editorMouseDrag(event.position);
}

void Effect2DExpandedEditorComponent::mouseUp(const MouseEvent& event) {
    ignoreUnused(event);
    editorMouseUp();
    commitTransaction();
}

Rectangle<float> Effect2DExpandedEditorComponent::panelBoundsForAutomation() const {
    return editorPanelBounds();
}

var Effect2DExpandedEditorComponent::automationState() const {
    auto* root = new DynamicObject();
    root->setProperty("panelBounds", rectangleToVar(editorPanelBounds()));
    root->setProperty("controlBounds", rectangleToVar(editorControlBounds()));
    root->setProperty("vertexCount", widget.vertexCountForAutomation());
    root->setProperty("panelState", widget.automationState());
    appendEditorAutomation(*root);
    return root;
}

bool Effect2DExpandedEditorComponent::editorMouseMove(Point<float>) { return false; }
bool Effect2DExpandedEditorComponent::editorMouseDown(Point<float>) { return false; }
bool Effect2DExpandedEditorComponent::editorMouseDrag(Point<float>) { return false; }
void Effect2DExpandedEditorComponent::editorMouseUp() {}

Rectangle<float> Effect2DExpandedEditorComponent::contentBounds() const {
    Rectangle<float> bounds = getLocalBounds().toFloat();
    bounds.removeFromTop(kHeaderHeight);
    return bounds.reduced(12.f, 10.f);
}

void Effect2DExpandedEditorComponent::publishCurrentState() {
    if (syncingControls || callbacks.publishState == nullptr) {
        return;
    }
    applyEditorStateToWidget();
    const uint64_t currentRevision = CurveNodeModelCodec::revisionFromParameters(node.parameters);
    const String snapshot = widget.prepareModelPublication(currentRevision);
    const auto controls = editorControls();
    if (!callbacks.publishState(snapshot, widget.modelRevision(), controls)) {
        return;
    }
    node.parameters = controls;
    node.parameters.push_back({ CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot", snapshot });
    node.parameters.push_back({ CurveNodeModelCodec::revisionParameterId(),
            "Curve Model Revision", String((int64_t) widget.modelRevision()) });
}

void Effect2DExpandedEditorComponent::beginTransaction() {
    if (!transactionActive && callbacks.beginTransaction != nullptr) {
        callbacks.beginTransaction();
        transactionActive = true;
    }
}

void Effect2DExpandedEditorComponent::commitTransaction() {
    if (transactionActive && callbacks.commitTransaction != nullptr) {
        callbacks.commitTransaction();
    }
    transactionActive = false;
}

void Effect2DExpandedEditorComponent::requestRepaint() {
    repaint();
    if (callbacks.repaintOpenGL != nullptr) {
        callbacks.repaintOpenGL();
    }
}

Rectangle<float> Effect2DExpandedEditorComponent::closeButtonBounds() const {
    const auto bounds = getLocalBounds().toFloat();
    return Rectangle<float>(22.f, 22.f).withCentre({ bounds.getRight() - 22.f, kHeaderHeight * 0.5f });
}

void Effect2DExpandedEditorComponent::updatePanelHost() {
    if (getWidth() <= 0 || getHeight() <= 0) {
        return;
    }
    Component* panel = widget.getExpandedPanelComponentIfCreated();
    if (panel == nullptr) {
        panel = widget.prepareExpandedPanelComponent(node, contentBounds());
    }
    if (panel == nullptr) {
        return;
    }
    if (panel->getParentComponent() != this) {
        addAndMakeVisible(panel);
    }
    panel->setBounds(editorPanelBounds().toNearestInt());
    panel->setVisible(true);
    panel->toFront(false);
    panel->repaint();
}

void Effect2DExpandedEditorComponent::persistEffectMeshState() {
    publishCurrentState();
    requestRepaint();
}

}
