#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/ZoomPanel.h>

#include "Nodes/Curve/Panel/ConcreteCurvePanels.h"
#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "UI/CanvasChromePalette.h"
#include "UI/ReferencePitchEditor.h"

using namespace gl;

namespace CycleV2 {

namespace {

juce::String noteLabel(double value) {
    const int note = juce::roundToInt(value);
    return juce::MidiMessage::getMidiNoteName(note, true, true, 4)
            + "  " + juce::String(note);
}

std::optional<double> parseNote(const juce::String& text) {
    const juce::String number = text.fromLastOccurrenceOf(" ", false, false).trim();
    if (number.isEmpty() || !number.containsOnly("0123456789")) {
        return std::nullopt;
    }
    const int note = number.getIntValue();
    return juce::isPositiveAndBelow(note, 128)
            ? std::optional<double>((double) note) : std::nullopt;
}

}

ReferencePitchEditor::ReferencePitchEditor(ReferenceLibrary& libraryToUse) :
        library     (libraryToUse)
    ,   fundamental (*this, "Fundamental") {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    EnvelopeMeshState::replacePointPath(mesh,
            { Vertex2(0.f, 0.5f), Vertex2(1.f, 0.5f) });
    panel = createEnvelopeCurvePanel(
            &environment.services().getRepo(), environment.services(), mesh);
    panel->setEnvelopeAxisLinks(true, true);
    panel->setEnvelopeBipolar(false);
    host = std::make_unique<CurvePanelHost>(panel->hostedPanel(),
            static_cast<CurvePanelHostDelegate&>(*this));
    addAndMakeVisible(host->component());

    fundamental.slider.setRange(0, 127, 1);
    fundamental.slider.setKeyboardSteps(1, 1);
    fundamental.configureValuePresentation(
            noteLabel,
            parseNote,
            60,
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
    host->releaseSharedGlResources();
    host.reset();
    panel.reset();
    mesh.destroy();
}

bool ReferencePitchEditor::open(const juce::String& referenceId) {
    if (library.analysisFor(referenceId) == nullptr) {
        return false;
    }
    openedId = referenceId;
    syncFromLibrary();
    return true;
}

void ReferencePitchEditor::syncFromLibrary() {
    const auto* analysis = library.analysisFor(openedId);
    if (analysis == nullptr) {
        return;
    }
    panel->clearInteractionState();
    EnvelopeMeshState::replacePointPath(mesh, analysis->pitchPath);
    panel->refreshRasterizer();
    panel->updateZoomBounds(true);
    panel->resetEnvelopeVerticalRange();
    fundamental.slider.setValue(analysis->sample->fundNote,
            juce::NotificationType::dontSendNotification);
    fundamental.refreshValueText();
    if (onRenderRequested) {
        onRenderRequested();
    }
    repaint();
}

void ReferencePitchEditor::renderOpenGL(float scaleFactor) {
    if (!isVisible() || host->componentIfCreated() == nullptr) {
        return;
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
    host->releaseSharedGlResources();
}

juce::var ReferencePitchEditor::exportAutomationState() const {
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("referenceId", openedId);
    result->setProperty("visible", isVisible());
    result->setProperty("pointCount", (int) mesh.getCubes().size());
    result->setProperty("note", (int) fundamental.slider.getValue());
    if (const auto* analysis = library.analysisFor(openedId)) {
        result->setProperty("cyclogramRevision", (int64) analysis->cyclogramRevision);
    }
    return { result.release() };
}

juce::Rectangle<int> ReferencePitchEditor::curveBounds() const {
    if (const Component* component = host->componentIfCreated()) {
        return component->getBounds();
    }
    return {};
}

void ReferencePitchEditor::paint(juce::Graphics& graphics) {
    graphics.fillAll(CanvasChromePalette::canvasBackground);
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(13.f);
    const auto* record = library.selectedRecord();
    const juce::String title = record != nullptr
            ? "PITCH ENVELOPE  /  " + record->name : "PITCH ENVELOPE";
    graphics.drawText(title, 12, 5, juce::jmin(320, getWidth() / 3), 26,
            juce::Justification::centredLeft, true);
    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(17.f);
    graphics.drawText(juce::String::charToString(0x00d7), getWidth() - 36, 5, 28, 26,
            juce::Justification::centred);
}

void ReferencePitchEditor::resized() {
    const int rightInset = 44;
    const int noteWidth = juce::jmin(370, juce::jmax(270, getWidth() / 3));
    fundamental.setBounds({ getWidth() - rightInset - noteWidth, 4, noteWidth, 28 },
            92, 8, 70);
    if (Component* component = host->componentIfCreated()) {
        component->setBounds(8, 38, juce::jmax(0, getWidth() - 16),
                juce::jmax(0, getHeight() - 46));
    }
    if (onRenderRequested) {
        onRenderRequested();
    }
}

void ReferencePitchEditor::mouseUp(const juce::MouseEvent& event) {
    if (event.position.x >= (float) getWidth() - 38.f && event.position.y < 34.f
            && onClose) {
        onClose();
    }
}

bool ReferencePitchEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey && onClose) {
        onClose();
        return true;
    }
    const bool command = key.getModifiers().isCommandDown();
    if (command && key.getKeyCode() == 'z') {
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
    panel->setControlValues(true, 0.5f, 0.5f, 0.5f, 0);
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
