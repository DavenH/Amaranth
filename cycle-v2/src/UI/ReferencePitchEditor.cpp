#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/ZoomPanel.h>
#include <Util/LogRegionMapping.h>

#include "Nodes/Curve/Panel/ConcreteCurvePanels.h"
#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "UI/CanvasChromePalette.h"
#include "UI/ReferencePitchEditor.h"
#include "UI/ComparisonSurfaceView.h"
#include "UI/CanvasChromeMetrics.h"

using namespace gl;

namespace CycleV2 {

namespace {

juce::String noteLabel(double value) {
    const int note = juce::roundToInt(value) - LogRegionMapping::legacyMidiNoteBias;
    const auto name = note >= 0
            ? juce::MidiMessage::getMidiNoteName(note, true, true, 4) : juce::String("MIDI");
    return name + "  " + juce::String(note);
}

std::optional<double> parseNote(const juce::String& text) {
    const juce::String number = text.fromLastOccurrenceOf(" ", false, false).trim();
    const auto digits = number.startsWithChar('-') ? number.substring(1) : number;
    if (digits.isEmpty() || !digits.containsOnly("0123456789")) {
        return std::nullopt;
    }
    const int standardNote = number.getIntValue();
    if (standardNote < -LogRegionMapping::legacyMidiNoteBias
            || standardNote >= 128 - LogRegionMapping::legacyMidiNoteBias) {
        return std::nullopt;
    }
    return (double) (standardNote + LogRegionMapping::legacyMidiNoteBias);
}

}

ReferencePitchEditor::ReferencePitchEditor(ReferenceLibrary& libraryToUse) :
        library     (libraryToUse)
    ,   chrome      (*this, "Wave Reference", "referencePitch", [this] {
            if (onClose) {
                onClose();
            }
        })
    ,   cyclogram   (std::make_unique<ComparisonSurfaceView>())
    ,   fundamental (*this, "Fundamental") {
    setOpaque(true);
    addAndMakeVisible(*cyclogram);
    setWantsKeyboardFocus(true);
    EnvelopeMeshState::replacePointPath(mesh,
            { Vertex2(0.f, 0.5f), Vertex2(1.f, 0.5f) });
    panel = createEnvelopeCurvePanel(
            &environment.services().getRepo(), environment.services(), mesh);
    panel->setEnvelopeAxisLinks(true, true);
    panel->setEnvelopeBipolar(false);
    host = std::make_unique<CurvePanelHost>(panel->hostedPanel(),
            static_cast<CurvePanelHostDelegate&>(*this));
    auto* zoom = static_cast<ZoomPanel*>(host->zoomComponent());
    auto context = zoom->getZoomContext();
    context.haveHorz = false;
    zoom->setZoomContext(context);
    zoom->getComponent(true)->setVisible(false);
    auto* vertical = static_cast<juce::ScrollBar*>(zoom->getComponent(false));
    vertical->setTitle("Pitch range");
    vertical->addListener(this);
    addAndMakeVisible(zoom);

    fundamental.slider.setRange(0, 127, 1);
    fundamental.slider.setKeyboardSteps(1, 1);
    fundamental.configureValuePresentation(
            noteLabel,
            parseNote,
            60 + LogRegionMapping::legacyMidiNoteBias,
            1,
            1,
            "The reference's detected fundamental and comparison MIDI note");
    fundamental.slider.onDragStart = [this] { draggingFundamental = true; };
    fundamental.slider.onDragEnd = [this] {
        draggingFundamental = false;
        commitFundamental();
    };
    fundamental.slider.onValueChange = [this] {
        if (!draggingFundamental) {
            commitFundamental();
        }
    };
}

ReferencePitchEditor::~ReferencePitchEditor() {
    static_cast<juce::ScrollBar*>(panel->hostedPanel().getZoomPanel()->getComponent(false))
            ->removeListener(this);
    cyclogram->releaseOpenGLResources();
    host->releaseSharedGlResources();
    host.reset();
    panel.reset();
    mesh.destroy();
}

bool ReferencePitchEditor::open(const juce::String& referenceId) {
    if (library.analysisFor(referenceId) == nullptr) {
        return false;
    }
    if (openedId != referenceId) {
        displayedSample = nullptr;
        displayedRevision = 0;
    }
    openedId = referenceId;
    if (const auto* record = library.selectedRecord()) {
        chrome.setTitle(record->name);
    }
    syncFromLibrary();
    return true;
}

void ReferencePitchEditor::syncFromLibrary() {
    const auto* analysis = library.analysisFor(openedId);
    if (analysis == nullptr) {
        return;
    }
    refreshCyclogram();
    panel->clearInteractionState();
    EnvelopeMeshState::replacePointPath(mesh, analysis->pitchPath);
    prepareCurvePanel();
    panel->updateZoomBounds(true);
    panel->fitEnvelopeVerticalRange();
    fundamental.slider.setValue(analysis->sample->fundNote,
            juce::NotificationType::dontSendNotification);
    fundamental.refreshValueText();
    if (onRenderRequested) {
        onRenderRequested();
    }
    repaint();
}

void ReferencePitchEditor::refreshCyclogram() {
    const auto* analysis = library.analysisFor(openedId);
    if (analysis == nullptr || (displayedSample == analysis->sample.get()
            && displayedRevision == analysis->cyclogramRevision)) {
        return;
    }
    cyclogram->setGrid(analysis->cyclogram, (size_t) analysis->columns,
            (size_t) analysis->rows, PortDomain::TimeSignal,
            analysis->standardMidiNote(), 2.f);
    displayedSample = analysis->sample.get();
    displayedRevision = analysis->cyclogramRevision;
}

void ReferencePitchEditor::renderOpenGL(float scaleFactor) {
    if (!isVisible() || host->componentIfCreated() == nullptr) {
        return;
    }
    if (cyclogram->renderOpenGL(scaleFactor)) {
        juce::Component::SafePointer<ReferencePitchEditor> safeThis(this);
        juce::MessageManager::callAsync([safeThis] {
            if (safeThis != nullptr) {
                safeThis->repaint();
            }
        });
    }
    const auto bounds = host->componentIfCreated()->getLocalBounds().toFloat();
    if (bounds.getWidth() < 2.f || bounds.getHeight() < 2.f) {
        return;
    }

    GLint previousViewport[4] {};
    GLint previousMatrixMode {};
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);
    glViewport(0, 0,
            juce::roundToInt(bounds.getWidth() * scaleFactor),
            juce::roundToInt(bounds.getHeight() * scaleFactor));
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, bounds.getWidth(), bounds.getHeight(), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    host->render(bounds, bounds, scaleFactor);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode((GLenum) previousMatrixMode);
    glViewport(previousViewport[0], previousViewport[1],
            previousViewport[2], previousViewport[3]);
}

void ReferencePitchEditor::releaseOpenGLResources() {
    cyclogram->releaseOpenGLResources();
    host->releaseSharedGlResources();
}

juce::var ReferencePitchEditor::exportAutomationState() const {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("referenceId", openedId);
    result->setProperty("visible", isVisible());
    const auto rect = [](juce::Rectangle<int> bounds) {
        auto* value = new juce::DynamicObject();
        value->setProperty("x", bounds.getX());
        value->setProperty("y", bounds.getY());
        value->setProperty("width", bounds.getWidth());
        value->setProperty("height", bounds.getHeight());
        return juce::var(value);
    };
    result->setProperty("cyclogramBounds", rect(cyclogramBounds()));
    result->setProperty("curveBounds", rect(curveBounds()));
    result->setProperty("curveView", panel->automationState());
    result->setProperty("zoomControls", host->zoomControlsState());
    result->setProperty("surfaceView", cyclogram->automationDiagnostics());
    result->setProperty("pointCount", (int) mesh.getCubes().size());
    result->setProperty("note", (int) fundamental.slider.getValue()
            - LogRegionMapping::legacyMidiNoteBias);
    if (const auto* analysis = library.analysisFor(openedId)) {
        result->setProperty("cyclogramRevision", (int64) analysis->cyclogramRevision);
    }
    return { result.release() };
}

juce::Rectangle<int> ReferencePitchEditor::curveBounds() const {
    if (const Component* component = host->componentIfCreated()) {
        return getLocalArea(component, component->getLocalBounds());
    }
    return {};
}

juce::Rectangle<int> ReferencePitchEditor::verticalZoomBounds() const {
    const auto* scrollbar = panel->hostedPanel().getZoomPanel()->getComponent(false);
    return getLocalArea(scrollbar, scrollbar->getLocalBounds());
}

juce::Rectangle<int> ReferencePitchEditor::cyclogramBounds() const {
    return cyclogram->getBounds();
}

juce::Rectangle<int> ReferencePitchEditor::closeBounds() const {
    const auto* button = findChildWithID("referencePitch.close");
    return button != nullptr ? button->getBounds() : juce::Rectangle<int>();
}

void ReferencePitchEditor::paint(juce::Graphics& graphics) {
    chrome.paint(graphics);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(13.f);
    graphics.drawText("Pitch envelope", 12, cyclogram->getBottom() + 6, 130, 28,
            juce::Justification::centredLeft);
}

void ReferencePitchEditor::resized() {
    chrome.resized();
    auto bounds = getLocalBounds().withTrimmedTop(CanvasChromeMetrics::fullEditorHeaderHeight)
            .reduced(12, 8);
    const int pitchHeight = juce::jlimit(90, 170, bounds.getHeight() / 3);
    cyclogram->setBounds(bounds.removeFromTop(juce::jmax(0, bounds.getHeight() - pitchHeight - 34))
            .withTrimmedRight(ZoomPanel::scrollbarWidth));
    bounds.removeFromTop(6);
    auto heading = bounds.removeFromTop(28);
    const int noteWidth = juce::jmin(320, juce::jmax(0, heading.getWidth() - 145));
    fundamental.setBounds(heading.removeFromRight(noteWidth), 92, 8, 70);
    if (Component* component = host->zoomComponentIfCreated()) {
        component->setBounds(bounds);
    }
    if (onRenderRequested) {
        onRenderRequested();
    }
}

void ReferencePitchEditor::scrollBarMoved(juce::ScrollBar*, double) {
    // Let ZoomPanel apply the scrollbar range before the GL thread reads it.
    juce::Component::SafePointer<ReferencePitchEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis != nullptr && safeThis->onRenderRequested) {
            safeThis->onRenderRequested();
        }
    });
}

bool ReferencePitchEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && onClose) {
        onClose();
        return true;
    }
    const bool command = key.getModifiers().isCommandDown();
    const int keyCode = key.getKeyCode();
    if (command && (keyCode == 'z' || keyCode == 'Z')) {
        const bool changed = key.getModifiers().isShiftDown()
                ? library.redoPitchEdit() : library.undoPitchEdit();
        if (changed) {
            syncFromLibrary();
            if (onEdited) {
                onEdited();
            }
        }
        return changed;
    }
    return false;
}

void ReferencePitchEditor::initialiseCurvePanel(juce::Component* component) {
    panel->initWithHost(component);
}

void ReferencePitchEditor::updateCurvePanelZoom(bool resetView) {
    panel->updateZoomBounds(resetView);
}

void ReferencePitchEditor::prepareCurvePanel() {
    // Point paths occupy the static time slice, matching EnvelopeMeshState.
    panel->setControlValues(true, 0.5f, 0.5f, 0.f, 0);
    panel->refreshRasterizer();
}

void ReferencePitchEditor::synchronizeCurvePanelSelection() {
}

void ReferencePitchEditor::repaintCurvePanel() {
    if (Component* component = host->componentIfCreated()) {
        component->repaint();
    }
}

void ReferencePitchEditor::setCurvePanelCursor(const juce::MouseCursor& cursor) {
    if (Component* component = host->componentIfCreated()) {
        component->setMouseCursor(cursor);
    }
}

void ReferencePitchEditor::beginEdit() {
    editingMesh = false;
}

void ReferencePitchEditor::publishIntermediateRevision() {
    editingMesh = true;
    if (onRenderRequested) {
        onRenderRequested();
    }
}

void ReferencePitchEditor::commitEdit() {
    if (!editingMesh) {
        return;
    }
    editingMesh = false;
    if (library.replacePitchPath(EnvelopeMeshState::pointPath(mesh))) {
        if (onEdited) {
            onEdited();
        }
    } else {
        syncFromLibrary();
    }
}

bool ReferencePitchEditor::cancelEdit() {
    editingMesh = false;
    syncFromLibrary();
    return true;
}

void ReferencePitchEditor::commitFundamental() {
    if (library.setFundamental((int) fundamental.slider.getValue()) && onEdited) {
        onEdited();
    }
}

}
