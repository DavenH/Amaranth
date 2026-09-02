#include "Nodes/Curve/Editor/CurveExpandedEditorComponent.h"

#include "Nodes/Curve/Editor/CurveEditorPrimitives.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Runtime/FingerprintBuilder.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };

class EditorCloseButton final : public Button {
public:
    EditorCloseButton() : Button("Close editor") {
        setTitle("Close editor");
        setDescription("Closes the expanded editor");
        setTooltip("Close editor");
        setWantsKeyboardFocus(true);
        setMouseCursor(MouseCursor::PointingHandCursor);
    }

    void paintButton(Graphics& graphics, bool highlighted, bool down) override {
        const Rectangle<float> bounds = getLocalBounds().toFloat().reduced(0.5f);
        graphics.setColour(Colour(0xff0e1318).brighter(down ? 0.12f : highlighted ? 0.06f : 0.f));
        graphics.fillEllipse(bounds);
        graphics.setColour(hasKeyboardFocus(false) ? Colour(0xff65b8ff) : Colour(0xff354050));
        graphics.drawEllipse(
                bounds,
                hasKeyboardFocus(false)
                        ? CanvasChromeMetrics::focusRingWidth
                        : CanvasChromeMetrics::restingBorderWidth);
        graphics.setColour(kText);
        graphics.drawLine(7.f, 7.f, bounds.getRight() - 7.f, bounds.getBottom() - 7.f, 1.4f);
        graphics.drawLine(bounds.getRight() - 7.f, 7.f, 7.f, bounds.getBottom() - 7.f, 1.4f);
    }
};

var rectangleToVar(Rectangle<float> bounds) {
    auto* object = new DynamicObject();
    object->setProperty("x", bounds.getX());
    object->setProperty("y", bounds.getY());
    object->setProperty("width", bounds.getWidth());
    object->setProperty("height", bounds.getHeight());
    return object;
}

}

CurveExpandedEditorComponent::CurveExpandedEditorComponent(CurveEditorWidget& targetWidget) :
        widget(targetWidget)
    ,   closeButton(std::make_unique<EditorCloseButton>()) {
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    closeButton->setComponentID("curveEditor.close");
    closeButton->onClick = [this] {
        if (delegate != nullptr) {
            delegate->closeCurveEditor();
        }
    };
    addAndMakeVisible(*closeButton);
}

CurveExpandedEditorComponent::~CurveExpandedEditorComponent() {
    widget.setDelegate(nullptr);
}

void CurveExpandedEditorComponent::setDelegate(CurveExpandedEditorDelegate* nextDelegate) {
    delegate = nextDelegate;
    widget.setDelegate(this);
}

void CurveExpandedEditorComponent::setStatusMessage(const String& message) {
    if (delegate != nullptr) {
        delegate->setCurveEditorStatus(message);
    }
}

bool CurveExpandedEditorComponent::setAudioResource(NodeAudioResourceEdit edit) {
    return delegate != nullptr && delegate->setAudioResource(std::move(edit));
}

bool CurveExpandedEditorComponent::removeAudioResource() {
    return delegate != nullptr && delegate->removeAudioResource();
}

std::optional<NodeAudioResourceSummary> CurveExpandedEditorComponent::audioResourceSummary() const {
    return delegate != nullptr ? delegate->audioResourceSummary() : std::nullopt;
}

void CurveExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    setEditorModelState(node.model);
    widget.syncFromNode(node);
    const ScopedValueSetter<bool> guard(syncingControls, true);
    syncEditorFromNode();
    applyEditorStateToWidget();
    refreshEditorSubject();
}

void CurveExpandedEditorComponent::refreshEditorSubject() {
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
    graphics.fillRoundedRectangle(
            outer.translated(0.f, 10.f),
            CanvasChromeMetrics::panelCornerRadius);
    graphics.setColour(Colour(0xff141a21));
    graphics.fillRoundedRectangle(outer, CanvasChromeMetrics::panelCornerRadius);
    graphics.restoreState();

    const auto headerLayout = embeddedEditorHeaderLayout(outer, headerAction != nullptr);
    const Rectangle<float> header = headerLayout.header;
    graphics.setColour(Colour(0xff202833));
    graphics.fillRoundedRectangle(header, CanvasChromeMetrics::panelCornerRadius);
    graphics.fillRect(header.withTrimmedTop(
            header.getHeight() - CanvasChromeMetrics::panelCornerRadius));
    graphics.setColour(kText);
    graphics.setFont(FontOptions(CanvasChromeMetrics::sectionTitleFontSize));
    graphics.drawText(
            title.isEmpty() ? labelForNodeKind(node.kind) : title,
            headerLayout.title,
            Justification::centredLeft);

    paintEditor(graphics);

    graphics.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
    graphics.drawRoundedRectangle(
            getLocalBounds().toFloat().reduced(0.75f),
            CanvasChromeMetrics::panelCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
}

void CurveExpandedEditorComponent::resized() {
    closeButton->setBounds(closeButtonBounds().toNearestInt());
    if (headerAction != nullptr) {
        headerAction->setBounds(embeddedEditorHeaderLayout(
                getLocalBounds().toFloat(), true).enabled.toNearestInt());
    }
    updatePanelHost();
    layoutEditor();
}

void CurveExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    if (!editorMouseMove(event.position)) {
        setMouseCursor(MouseCursor::NormalCursor);
    }
}

void CurveExpandedEditorComponent::mouseDown(const MouseEvent& event) {
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
    root->setProperty(
            "headerActionBounds",
            rectangleToVar(headerAction != nullptr
                    ? headerAction->getBounds().toFloat()
                    : Rectangle<float>()));
    root->setProperty("vertexCount", widget.vertexCountForAutomation());
    root->setProperty("panelState", widget.automationState());
    appendEditorAutomation(*root);
    return root;
}

bool CurveExpandedEditorComponent::editorMouseMove(Point<float>) {
    return false;
}

bool CurveExpandedEditorComponent::editorMouseDown(Point<float>) {
    return false;
}

bool CurveExpandedEditorComponent::editorMouseDrag(Point<float>) {
    return false;
}

void CurveExpandedEditorComponent::editorMouseUp() {
}

Rectangle<float> CurveExpandedEditorComponent::contentBounds() const {
    Rectangle<float> bounds = getLocalBounds().toFloat();
    bounds.removeFromTop(CanvasChromeMetrics::embeddedEditorHeaderHeight);
    return bounds.reduced(12.f, 10.f);
}

void CurveExpandedEditorComponent::publishCurrentState() {
    if (syncingControls || delegate == nullptr) {
        return;
    }
    applyEditorStateToWidget();
    requestRepaint();
    if (transactionActive) {
        if (!publishModelState()) {
            return;
        }
        transientStateChanged = true;
        FingerprintBuilder fingerprint(widget.contentRevision());
        for (const auto& control : editorControls()) {
            fingerprint.add(control.id).add(control.value);
        }
        delegate->curveTransientStateChanged(fingerprint.value());
        return;
    }
    publishModelState();
}

bool CurveExpandedEditorComponent::publishModelState() {
    const uint64_t currentRevision = transactionActive
            ? transactionBaseRevision
            : (editorModel != nullptr ? editorModel->revision() : 0);
    const auto publication = widget.prepareModelPublication(currentRevision);
    const auto controls = editorControls();
    if (publication == nullptr || !delegate->publishCurveState(publication, controls)) {
        return false;
    }
    node.parameters = controls;
    editorModel = publication;
    node.model = std::move(publication);
    return true;
}

void CurveExpandedEditorComponent::beginTransaction() {
    if (!transactionActive && delegate != nullptr) {
        transactionBaseRevision = editorModel != nullptr ? editorModel->revision() : 0;
        delegate->beginCurveTransaction();
        transactionActive = true;
        transientStateChanged = false;
    }
}

void CurveExpandedEditorComponent::setEditorModelState(NodeModelStatePtr model) {
    editorModel = std::move(model);
}

void CurveExpandedEditorComponent::commitTransaction() {
    if (transactionActive && delegate != nullptr) {
        delegate->commitCurveTransaction();
    }
    transactionActive = false;
    transientStateChanged = false;
}

void CurveExpandedEditorComponent::requestRepaint() {
    repaint();
    if (delegate != nullptr) {
        delegate->repaintCurveEditorOpenGL();
    }
}

void CurveExpandedEditorComponent::bindContinuousControl(LabeledParameterSlider& control) {
    control.slider.onValueChange = [this] {
        publishCurrentState();
    };
    control.slider.onDragStart = [this] {
        beginTransaction();
    };
    control.slider.onDragEnd = [this] {
        commitTransaction();
    };
}

void CurveExpandedEditorComponent::bindContinuousControls(
        std::initializer_list<LabeledParameterSlider*> controls) {
    for (auto* control : controls) {
        bindContinuousControl(*control);
    }
}

void CurveExpandedEditorComponent::bindDiscreteControl(ParameterToggle& control) {
    bindDiscreteAction(control.button, [] {});
}

void CurveExpandedEditorComponent::bindDiscreteControl(ComboBox& control) {
    control.onChange = [this] {
        publishDiscreteControlChange();
    };
}

void CurveExpandedEditorComponent::publishDiscreteControlChange() {
    auto noOperation = [] {};
    performDiscreteEdit(noOperation);
}

void CurveExpandedEditorComponent::setHeaderAction(Component& action) {
    headerAction = &action;
    addAndMakeVisible(action);
    action.setBounds(embeddedEditorHeaderLayout(
            getLocalBounds().toFloat(), true).enabled.toNearestInt());
    repaint();
}

Rectangle<float> CurveExpandedEditorComponent::closeButtonBounds() const {
    return embeddedEditorHeaderLayout(
            getLocalBounds().toFloat(), headerAction != nullptr).close;
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
}

void CurveExpandedEditorComponent::repaintCurvePanelController() {
    syncInteractionControls();
    requestRepaint();
}

void CurveExpandedEditorComponent::beginCurvePanelControllerEdit() {
    beginTransaction();
}

void CurveExpandedEditorComponent::curvePanelControllerEdited() {
    persistEffectMeshState();
}

void CurveExpandedEditorComponent::commitCurvePanelControllerEdit() {
    syncInteractionControls();
    commitTransaction();
}

}
