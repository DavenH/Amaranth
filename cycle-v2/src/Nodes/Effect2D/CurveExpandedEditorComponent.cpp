#include "CurveExpandedEditorComponent.h"

#include "CurveEditorPrimitives.h"
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

CurveExpandedEditorComponent::CurveExpandedEditorComponent(Effect2DWidget& targetWidget) :
        widget(targetWidget) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
}

CurveExpandedEditorComponent::~CurveExpandedEditorComponent() {
    widget.setDelegate(nullptr);
}

void CurveExpandedEditorComponent::setDelegate(CurveExpandedEditorDelegate* nextDelegate) {
    delegate = nextDelegate;
    widget.setDelegate(this);
}

void CurveExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    const ScopedValueSetter<bool> guard(syncingControls, true);
    syncEditorFromNode();
    applyEditorStateToWidget();
    updatePanelHost();
    layoutEditor();
    repaint();
}

void CurveExpandedEditorComponent::renderOpenGL(float scaleFactor) {
    widget.renderExpandedPanelOpenGL(
            node,
            editorPanelBounds().translated((float) getX(), (float) getY()),
            getLocalBounds().toFloat().translated((float) getX(), (float) getY()),
            scaleFactor);
}

void CurveExpandedEditorComponent::paint(Graphics& graphics) {
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

void CurveExpandedEditorComponent::resized() {
    updatePanelHost();
    layoutEditor();
}

void CurveExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    if (!editorMouseMove(event.position)) {
        setMouseCursor(closeButtonBounds().contains(event.position)
                ? MouseCursor::PointingHandCursor
                : MouseCursor::NormalCursor);
    }
}

void CurveExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    if (closeButtonBounds().contains(event.position)) {
        if (delegate != nullptr) {
            delegate->closeEffect2DEditor();
        }
        return;
    }
    beginTransaction();
    editorMouseDown(event.position);
}

void CurveExpandedEditorComponent::mouseDrag(const MouseEvent& event) {
    editorMouseDrag(event.position);
}

void CurveExpandedEditorComponent::mouseUp(const MouseEvent& event) {
    ignoreUnused(event);
    editorMouseUp();
    commitTransaction();
}

Rectangle<float> CurveExpandedEditorComponent::panelBoundsForAutomation() const {
    return editorPanelBounds();
}

var CurveExpandedEditorComponent::automationState() const {
    auto* root = new DynamicObject();
    root->setProperty("panelBounds", rectangleToVar(editorPanelBounds()));
    root->setProperty("controlBounds", rectangleToVar(editorControlBounds()));
    root->setProperty("vertexCount", widget.vertexCountForAutomation());
    root->setProperty("panelState", widget.automationState());
    appendEditorAutomation(*root);
    return root;
}

bool CurveExpandedEditorComponent::editorMouseMove(Point<float>) { return false; }
bool CurveExpandedEditorComponent::editorMouseDown(Point<float>) { return false; }
bool CurveExpandedEditorComponent::editorMouseDrag(Point<float>) { return false; }
void CurveExpandedEditorComponent::editorMouseUp() {}

Rectangle<float> CurveExpandedEditorComponent::contentBounds() const {
    Rectangle<float> bounds = getLocalBounds().toFloat();
    bounds.removeFromTop(kHeaderHeight);
    return bounds.reduced(12.f, 10.f);
}

void CurveExpandedEditorComponent::publishCurrentState() {
    if (syncingControls || delegate == nullptr) {
        return;
    }
    applyEditorStateToWidget();
    const uint64_t currentRevision = CurveNodeModelCodec::revisionFromParameters(node.parameters);
    const String snapshot = widget.prepareModelPublication(currentRevision);
    const auto controls = editorControls();
    if (!delegate->publishEffect2DState(snapshot, widget.modelRevision(), controls)) {
        return;
    }
    node.parameters = controls;
    node.parameters.push_back({ CurveNodeModelCodec::snapshotParameterId(), "Curve Model Snapshot", snapshot });
    node.parameters.push_back({ CurveNodeModelCodec::revisionParameterId(),
            "Curve Model Revision", String((int64_t) widget.modelRevision()) });
}

void CurveExpandedEditorComponent::beginTransaction() {
    if (!transactionActive && delegate != nullptr) {
        delegate->beginEffect2DTransaction();
        transactionActive = true;
    }
}

void CurveExpandedEditorComponent::commitTransaction() {
    if (transactionActive && delegate != nullptr) {
        delegate->commitEffect2DTransaction();
    }
    transactionActive = false;
}

void CurveExpandedEditorComponent::requestRepaint() {
    repaint();
    if (delegate != nullptr) {
        delegate->repaintEffect2DEditorOpenGL();
    }
}

void CurveExpandedEditorComponent::bindContinuousControl(LabeledParameterSlider& control) {
    control.slider.onValueChange = [this] {
        publishCurrentState();
        requestRepaint();
    };
    control.slider.onDragStart = [this] {
        beginTransaction();
    };
    control.slider.onDragEnd = [this] {
        commitTransaction();
    };
}

void CurveExpandedEditorComponent::bindDiscreteControl(ParameterToggle& control) {
    bindDiscreteAction(control.button, [] {});
}

void CurveExpandedEditorComponent::bindDiscreteControl(ComboBox& control) {
    control.onChange = [this] {
        auto noOperation = [] {};
        performDiscreteEdit(noOperation);
    };
}

Rectangle<float> CurveExpandedEditorComponent::closeButtonBounds() const {
    const auto bounds = getLocalBounds().toFloat();
    return Rectangle<float>(22.f, 22.f).withCentre({ bounds.getRight() - 22.f, kHeaderHeight * 0.5f });
}

void CurveExpandedEditorComponent::updatePanelHost() {
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

void CurveExpandedEditorComponent::persistEffectMeshState() {
    publishCurrentState();
    requestRepaint();
}

void CurveExpandedEditorComponent::repaintCurvePanelController() {
    requestRepaint();
}

void CurveExpandedEditorComponent::setCurvePanelControllerCursor(const MouseCursor& cursor) {
    setMouseCursor(cursor);
}

void CurveExpandedEditorComponent::curvePanelControllerEdited() {
    persistEffectMeshState();
}

}
