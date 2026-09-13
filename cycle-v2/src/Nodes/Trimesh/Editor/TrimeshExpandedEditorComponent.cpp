#include "Nodes/Trimesh/Editor/TrimeshExpandedEditorComponent.h"

#include <utility>

#include "Graph/NodeParameterMap.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EditorChromeLayout.h"
#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
constexpr int kControlLabelWidth = 56;
constexpr int kSignalTypeSelectorWidth = 198;
constexpr int kPolaritySelectorWidth = 142;
constexpr int kControlGap = 8;
constexpr int kControlGroupGap = 14;
constexpr int kControlHeight = 26;

std::vector<PropertySegmentOption> signalTypeOptions() {
    return {
            { "Time", "time", "trimeshEditor.signalType.time",
                    "Emit a time-domain waveform" },
            { "Magnitude", "spectralMagnitude", "trimeshEditor.signalType.spectralMagnitude",
                    "Emit spectral magnitudes" },
            { "Phase", "spectralPhase", "trimeshEditor.signalType.spectralPhase",
                    "Emit spectral phase" }
    };
}

std::vector<PropertySegmentOption> polarityOptions() {
    return {
            { "Unipolar", "unipolar", "trimeshEditor.polarity.unipolar",
                    "Map magnitude values from zero to one" },
            { "Bipolar", "bipolar", "trimeshEditor.polarity.bipolar",
                    "Map magnitude values from minus one to one" }
    };
}

}

TrimeshExpandedEditorComponent::TrimeshExpandedEditorComponent(TrimeshWidget& targetWidget) :
        widget      (targetWidget)
    ,   controls    (targetWidget)
    ,   signalTypeSelector(signalTypeOptions())
    ,   polaritySelector(polarityOptions()) {
    setOpaque(false);
    setName("TrimeshExpandedEditor");
    setInterceptsMouseClicks(true, true);
    addAndMakeVisible(controls);
    enabled.setComponentID("trimeshEditor.enabled");
    enabled.onClick = [this] {
        if (delegate != nullptr) {
            delegate->setTrimeshEnabled(enabled.getToggleState());
        }
    };
    addAndMakeVisible(enabled);
    stylePropertyLabel(signalTypeLabel, "Type");
    signalTypeLabel.setJustificationType(Justification::centredRight);
    signalTypeSelector.setComponentID("trimeshEditor.signalType");
    signalTypeSelector.onChange = [this](const String& signalType) {
        if (delegate == nullptr || !delegate->setTrimeshSignalTypeValue(signalType)) {
            signalTypeSelector.setSelectedValue(
                    NodeParameterMap(node).stringValue("signalType", "time"));
            return;
        }
        updateSignalControls();
    };
    stylePropertyLabel(polarityLabel, "Polarity");
    polarityLabel.setJustificationType(Justification::centredRight);
    polaritySelector.setComponentID("trimeshEditor.polarity");
    polaritySelector.onChange = [this](const String& polarity) {
        if (delegate == nullptr || !delegate->setTrimeshPolarityValue(polarity)) {
            polaritySelector.setSelectedValue(
                    NodeParameterMap(node).stringValue("polarity", "unipolar"));
        }
    };
    addAndMakeVisible(signalTypeLabel);
    addAndMakeVisible(signalTypeSelector);
    addAndMakeVisible(polarityLabel);
    addAndMakeVisible(polaritySelector);
    widget.setExpandedPanelHostDelegate(this);
}

TrimeshExpandedEditorComponent::~TrimeshExpandedEditorComponent() {
    widget.setMorphEditGestureActive(false);
    widget.clearExpandedPanelHostDelegate(this);
}

void TrimeshExpandedEditorComponent::setDelegate(TrimeshExpandedEditorDelegate* nextDelegate) {
    delegate = nextDelegate;
    controls.setDelegate(this);
}

void TrimeshExpandedEditorComponent::setNode(const Node& nextNode) {
    node = nextNode;
    enabled.setToggleState(
            NodeParameterMap(node).boolValue("enabled", true),
            dontSendNotification);
    signalTypeSelector.setSelectedValue(
            NodeParameterMap(node).stringValue("signalType", "time"));
    polaritySelector.setSelectedValue(
            NodeParameterMap(node).stringValue("polarity", "unipolar"));
    if (!widget.isMeshEditGestureActive()) {
        updatePanelHosts();
    }
    updateControlsHost();
    updateSignalControls();
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
    controls.refreshHitRegions();
    updateSignalControls();
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

bool TrimeshExpandedEditorComponent::showsOutputScale() const {
    return controls.getOutputScaleSliderCount() == 1;
}

float TrimeshExpandedEditorComponent::spectralRangeValue() const {
    return NodeParameterMap(node).floatValue("range", 0.5f);
}

float TrimeshExpandedEditorComponent::outputScaleValue() const {
    return NodeParameterMap(node).floatValue(outputScaleParameterId(), 0.5f);
}

String TrimeshExpandedEditorComponent::outputScaleParameterId() const {
    return renderProfile.getSliceStyle().isSpectral() ? "range" : "gain";
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
    g.fillRoundedRectangle(
            panel.translated(0.f, 10.f),
            CanvasChromeMetrics::panelCornerRadius);
    g.setColour(Colour(0xff141a21));
    g.fillRoundedRectangle(panel, CanvasChromeMetrics::panelCornerRadius);
    g.restoreState();

    const auto headerLayout = embeddedEditorHeaderLayout(panel, true);
    const Rectangle<float> header = headerLayout.header;
    g.setColour(Colour(0xff202833));
    g.fillRoundedRectangle(header, CanvasChromeMetrics::panelCornerRadius);
    g.fillRect(header.withTrimmedTop(
            header.getHeight() - CanvasChromeMetrics::panelCornerRadius));

    g.setColour(kText);
    g.setFont(FontOptions(CanvasChromeMetrics::sectionTitleFontSize));
    Rectangle<float> titleBounds = headerLayout.title;
    const int controlsLeft = polarityLabel.isVisible()
            ? polarityLabel.getX()
            : signalTypeLabel.getX();
    titleBounds.setRight((float) controlsLeft - kControlGap);
    g.drawText(labelForNodeKind(node.kind), titleBounds, Justification::centredLeft);

    Rectangle<float> closeButton = closeButtonBounds();
    g.setColour(Colour(0xff0e1318));
    g.fillEllipse(closeButton);
    g.setColour(Colour(0xff354050));
    g.drawEllipse(closeButton, CanvasChromeMetrics::restingBorderWidth);
    g.setColour(kText);
    g.drawLine(closeButton.getX() + 7.f, closeButton.getY() + 7.f,
               closeButton.getRight() - 7.f, closeButton.getBottom() - 7.f, 1.4f);
    g.drawLine(closeButton.getRight() - 7.f, closeButton.getY() + 7.f,
               closeButton.getX() + 7.f, closeButton.getBottom() - 7.f, 1.4f);

    widget.setRenderProfile(renderProfile);
    widget.paintExpanded(g, node, content);

    panel = getLocalBounds().toFloat();
    g.setColour(Colour(0xffa7b0bd).withAlpha(0.62f));
    g.drawRoundedRectangle(
            panel.reduced(0.75f),
            CanvasChromeMetrics::panelCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
}

void TrimeshExpandedEditorComponent::resized() {
    enabled.setBounds(embeddedEditorHeaderLayout(
            getLocalBounds().toFloat(), true).enabled.toNearestInt());
    updateSignalControls();
    updatePanelHosts();
    updateControlsHost();
}

void TrimeshExpandedEditorComponent::mouseEnter(const MouseEvent& event) {
    updateCursor(event.position);
}

void TrimeshExpandedEditorComponent::mouseMove(const MouseEvent& event) {
    updateCursor(event.position);
}

void TrimeshExpandedEditorComponent::mouseDown(const MouseEvent& event) {
    const Point<float> position = event.position;
    if (closeButtonBounds().contains(position)) {
        if (delegate != nullptr) {
            delegate->closeTrimeshEditor();
        }
        return;
    }

    controls.beginPointerInteraction(
            position,
            Rectangle<int>(localPointToGlobal(position.roundToInt()), { 1, 1 }));
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
    widget.setMorphEditGestureActive(true);
    if (delegate != nullptr && delegate->beginTrimeshMorphEdit(id, value)) {
        setLocalMorphValue(id, value);
        return;
    }
    widget.setMorphEditGestureActive(false);
}

void TrimeshExpandedEditorComponent::updateTrimeshMorphControlEdit(float value) {
    if (delegate != nullptr && delegate->updateTrimeshMorphEdit(value)) {
        setLocalMorphValue({}, value);
    }
}

void TrimeshExpandedEditorComponent::endTrimeshMorphControlEdit() {
    if (delegate != nullptr) {
        delegate->endTrimeshMorphEdit();
    }
    widget.setMorphEditGestureActive(false);
    activeMorphParameterId = {};
}

void TrimeshExpandedEditorComponent::beginTrimeshOutputScaleControlEdit(
        const String& id,
        float value) {
    if (delegate != nullptr && delegate->beginTrimeshOutputScaleEdit(id, value)) {
        setLocalOutputScale(id, value);
    }
}

void TrimeshExpandedEditorComponent::updateTrimeshOutputScaleControlEdit(float value) {
    if (delegate != nullptr && delegate->updateTrimeshOutputScaleEdit(value)) {
        setLocalOutputScale(
                renderProfile.getSliceStyle().isSpectral() ? "range" : "gain",
                value);
    }
}

void TrimeshExpandedEditorComponent::endTrimeshOutputScaleControlEdit() {
    if (delegate != nullptr) {
        delegate->endTrimeshOutputScaleEdit();
    }
}

void TrimeshExpandedEditorComponent::setLocalOutputScale(const String& id, float value) {
    for (auto& parameter : node.parameters) {
        if (parameter.id == id) {
            parameter.value = String(jlimit(0.f, 1.f, value), 6);
            repaint();
            return;
        }
    }
}

void TrimeshExpandedEditorComponent::setLocalMorphValue(const String& id, float value) {
    const String parameterId = id.isNotEmpty() ? id : activeMorphParameterId;
    if (parameterId.isEmpty()) {
        return;
    }

    for (auto& parameter : node.parameters) {
        if (parameter.id == parameterId) {
            parameter.value = String(jlimit(0.f, 1.f, value), 6);
            activeMorphParameterId = parameterId;
            widget.syncFromNode(node);
            repaint();
            return;
        }
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
    controls.refreshSelectionState();
}

void TrimeshExpandedEditorComponent::requestTrimeshPanelRepaint() {
    controls.refreshSelectionState();
    repaint();

    if (delegate != nullptr) {
        delegate->repaintTrimeshEditorOpenGL();
    }
}

void TrimeshExpandedEditorComponent::setTrimeshPanelCursor(
        TrimeshPanelHostKind host,
        const MouseCursor& cursor) {
    if (host == TrimeshPanelHostKind::Panel2D) {
        panel2DCursor = cursor;
    } else {
        panel3DCursor = cursor;
    }
    setMouseCursor(cursor);
}

Rectangle<float> TrimeshExpandedEditorComponent::closeButtonBounds() const {
    return embeddedEditorHeaderLayout(getLocalBounds().toFloat(), true).close;
}

Rectangle<float> TrimeshExpandedEditorComponent::contentBounds() const {
    Rectangle<float> panel = getLocalBounds().toFloat();
    panel.removeFromTop(CanvasChromeMetrics::embeddedEditorHeaderHeight);
    return panel.reduced(10.f, 8.f);
}

String TrimeshExpandedEditorComponent::vertexGuideParameterField(const String& parameterId) const {
    return parameterId.fromLastOccurrenceOf(".", false, false);
}

MouseCursor TrimeshExpandedEditorComponent::cursorFor(Point<float> position) {
    if (closeButtonBounds().contains(position)) {
        return MouseCursor::PointingHandCursor;
    }

    MouseCursor controlsCursor = controls.cursorFor(position);
    if (controlsCursor != MouseCursor::NormalCursor) {
        return controlsCursor;
    }

    Component* panel2D = widget.getExpandedPanel2DComponentIfCreated();
    if (panel2D != nullptr && panel2D->getBounds().contains(position.roundToInt())) {
        return panel2DCursor;
    }

    Component* panel3D = widget.getExpandedPanel3DComponentIfCreated();
    if (panel3D != nullptr && panel3D->getBounds().contains(position.roundToInt())) {
        return panel3DCursor;
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
    enabled.toFront(false);
}

void TrimeshExpandedEditorComponent::updateControlsHost() {
    controls.setBounds(getLocalBounds());
    controls.setNode(node);
    controls.setContentBounds(contentBounds());
    controls.setVisible(node.kind == NodeKind::TrilinearMesh);
    controls.toFront(false);
    enabled.toFront(false);
}

void TrimeshExpandedEditorComponent::updateSignalControls() {
    signalTypeLabel.setBounds(signalTypeLabelBounds());
    signalTypeSelector.setBounds(signalTypeSelectorBounds());
    const bool showPolarity = signalTypeSelector.selectedValue() == "spectralMagnitude";
    polarityLabel.setVisible(showPolarity);
    polaritySelector.setVisible(showPolarity);
    if (showPolarity) {
        polarityLabel.setBounds(polarityLabelBounds());
        polaritySelector.setBounds(polaritySelectorBounds());
    }
    signalTypeLabel.toFront(false);
    signalTypeSelector.toFront(false);
    polarityLabel.toFront(false);
    polaritySelector.toFront(false);
    enabled.toFront(false);
}

Rectangle<int> TrimeshExpandedEditorComponent::polaritySelectorBounds() const {
    const Rectangle<int> typeLabel = signalTypeLabelBounds();
    return {
            typeLabel.getX() - kControlGroupGap - kPolaritySelectorWidth,
            typeLabel.getY(),
            kPolaritySelectorWidth,
            typeLabel.getHeight()
    };
}

Rectangle<int> TrimeshExpandedEditorComponent::polarityLabelBounds() const {
    const Rectangle<int> selector = polaritySelectorBounds();
    return {
            selector.getX() - kControlGap - kControlLabelWidth,
            selector.getY(),
            kControlLabelWidth,
            selector.getHeight()
    };
}

Rectangle<int> TrimeshExpandedEditorComponent::signalTypeSelectorBounds() const {
    const auto header = embeddedEditorHeaderLayout(
            getLocalBounds().toFloat(), true);
    return Rectangle<int>(
            kSignalTypeSelectorWidth,
            kControlHeight)
            .withCentre({
                    roundToInt(header.enabled.getX()
                            - CanvasChromeMetrics::embeddedEditorActionGap
                            - kSignalTypeSelectorWidth * 0.5f),
                    roundToInt(header.header.getCentreY())
            });
}

Rectangle<int> TrimeshExpandedEditorComponent::signalTypeLabelBounds() const {
    const Rectangle<int> selector = signalTypeSelectorBounds();
    return {
            selector.getX() - kControlGap - kControlLabelWidth,
            selector.getY(),
            kControlLabelWidth,
            selector.getHeight()
    };
}

}
