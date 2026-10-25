#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/ZoomPanel.h>

#include "Nodes/Curve/Panel/ConcreteCurvePanels.h"
#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "UI/ReferencePitchEditor.h"
#include "UI/ComparisonSurfaceView.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/EnvelopePurposeIconRenderer.h"
#include "UiIconData.h"

using namespace gl;

namespace CycleV2 {

ReferencePitchEditor::ReferencePitchEditor(ReferenceLibrary& libraryToUse) :
        library     (libraryToUse)
    ,   chrome      (*this, "Wave Reference", "referencePitch", [this] {
            if (onClose) {
                onClose();
            }
        })
    ,   cyclogram   (std::make_unique<ComparisonSurfaceView>())
    ,   timeView    ("Cyclogram", "Period-folded waveform", "Cyclogram", UiIconData::referenceCyclogram)
    ,   spectrumView("Spectrogram", "Harmonic spectrum over time", "Spectrogram", UiIconData::referenceSpectrogram)
    ,   zoomAttack  ("Zoom to attack", "Focus on the reference attack", "Zoom to attack", UiIconData::zoomAttack)
    ,   zoomFit     ("Fit pitch range", "Fit the pitch envelope vertically", "Fit pitch range", UiIconData::envelopeZoomFit)
    ,   zoomFull    ("Full reference view", "Show the full reference and pitch range", "Full view", UiIconData::zoomFull) {
    setOpaque(true);
    addAndMakeVisible(*cyclogram);
    setWantsKeyboardFocus(true);
    EnvelopeMeshState::replacePointPath(mesh,
            { Vertex2(0.f, 0.5f), Vertex2(1.f, 0.5f) });
    panel = createEnvelopeCurvePanel(
            &environment.services().getRepo(), environment.services(), mesh);
    panel->setEnvelopeAxisLinks(true, true);
    panel->setEnvelopeBipolar(false);
    panel->setEnvelopeReleaseEnabled(false);
    host = std::make_unique<CurvePanelHost>(panel->hostedPanel(),
            static_cast<CurvePanelHostDelegate&>(*this));
    auto* zoom = static_cast<ZoomPanel*>(host->zoomComponent());
    for (bool horizontal : { true, false }) {
        auto* bar = static_cast<juce::ScrollBar*>(zoom->getComponent(horizontal));
        bar->setTitle(horizontal ? "Reference time" : "Pitch range");
        bar->addListener(this);
    }
    addAndMakeVisible(zoom);

    configureViewControls();
}

void ReferencePitchEditor::configureViewControls() {
    cyclogram->setZoomBarsVisible(false);
    cyclogram->onCameraChanged = [this](const ZoomRect&) { syncTimeZoom(true); };
    for (auto* button : { &timeView, &spectrumView, &zoomAttack, &zoomFit, &zoomFull }) {
        addAndMakeVisible(button);
    }
    timeView.setComponentID("referencePitch.time");
    spectrumView.setComponentID("referencePitch.spectrum");
    zoomAttack.setComponentID("referencePitch.zoomAttack");
    zoomFit.setComponentID("referencePitch.zoomFit");
    zoomFull.setComponentID("referencePitch.zoomFull");
    timeView.setToggleState(true, juce::dontSendNotification);
    timeView.onClick = [this] { setSurfaceMode(false); };
    spectrumView.onClick = [this] { setSurfaceMode(true); };
    zoomAttack.onClick = [this] {
        panel->hostedPanel().getZoomPanel()->zoomToAttack();
        syncTimeZoom(false);
    };
    zoomFit.onClick = [this] {
        panel->fitEnvelopeVerticalRange();
        syncTimeZoom(false);
    };
    zoomFull.onClick = [this] { resetView(); };
}

ReferencePitchEditor::~ReferencePitchEditor() {
    for (bool horizontal : { true, false }) {
        static_cast<juce::ScrollBar*>(panel->hostedPanel().getZoomPanel()->getComponent(horizontal))
                ->removeListener(this);
    }
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
        surfaceCache.reset();
        displayedGeneration = -1;
        cyclogram->setCamera(ZoomRect());
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
    // Reference point paths occupy the static time slice.
    panel->setControlValues(true, 0.5f, 0.5f, 0.f, 0);
    panel->refreshRasterizer();
    panel->updateZoomBounds(true);
    panel->fitEnvelopeVerticalRange();
    syncTimeZoom(false);
    if (onRenderRequested) {
        onRenderRequested();
    }
    repaint();
}

void ReferencePitchEditor::refreshCyclogram() {
    const auto* analysis = library.analysisFor(openedId);
    if (analysis == nullptr) {
        return;
    }
    const bool desiredMode = library.spectralViewFor(openedId);
    if (spectral != desiredMode) {
        spectral = desiredMode;
        timeView.setToggleState(!spectral, juce::dontSendNotification);
        spectrumView.setToggleState(spectral, juce::dontSendNotification);
        auto camera = cyclogram->camera();
        camera.y = 0.f;
        camera.h = 1.f;
        cyclogram->setCamera(camera);
    }
    surfaceCache.refresh(*analysis);
    if (displayedGeneration == surfaceCache.generation() && displayedSpectral == spectral) {
        return;
    }
    const auto& surface = surfaceCache.surface(spectral);
    cyclogram->setGrid(surface.values, surface.gridColumns, surface.gridRows,
            surface.domain, analysis->standardMidiNote(), spectral ? 1.f : 2.f,
            false, analysis->sample->samplerate, surfaceCache.columnPositions(), (size_t) analysis->maximumRows);
    displayedGeneration = surfaceCache.generation();
    displayedSpectral = spectral;
}

void ReferencePitchEditor::setSurfaceMode(bool shouldUseSpectrum) {
    if (!library.setSpectralView(openedId, shouldUseSpectrum)) {
        return;
    }
    refreshCyclogram();
    syncTimeZoom(true);
    if (onViewModeChanged) {
        onViewModeChanged();
    }
}

void ReferencePitchEditor::syncTimeZoom(bool fromSurface) {
    if (linkingTimeZoom) {
        return;
    }
    const juce::ScopedValueSetter<bool> linking(linkingTimeZoom, true);
    auto* zoom = panel->hostedPanel().getZoomPanel();
    auto camera = cyclogram->camera();
    if (fromSurface) {
        zoom->rect.x = camera.x;
        zoom->rect.w = camera.w;
        zoom->panelZoomChanged(false);
    } else {
        camera.x = zoom->rect.x;
        camera.w = zoom->rect.w;
        cyclogram->setCamera(camera);
    }
    if (onRenderRequested) {
        onRenderRequested();
    }
}

void ReferencePitchEditor::resetView() {
    panel->hostedPanel().getZoomPanel()->zoomToFull();
    cyclogram->setCamera(ZoomRect());
    syncTimeZoom(false);
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
    result->setProperty("surfaceMode", spectral ? "spectrum" : "time");
    result->setProperty("surfaceGeneration", surfaceCache.generation());
    result->setProperty("spectrumGeneration", surfaceCache.spectrumGeneration());
    result->setProperty("pointCount", (int) mesh.getCubes().size());
    if (const auto* analysis = library.analysisFor(openedId)) {
        result->setProperty("note", analysis->standardMidiNote());
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

std::vector<std::pair<juce::String, juce::Rectangle<int>>>
ReferencePitchEditor::pointerTargetsForAutomation() const {
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> targets {
            { "referencePitch.curve", curveBounds() },
            { "referencePitch.cyclogram", cyclogramBounds() },
            { "referencePitch.close", closeBounds() }
    };
    for (const auto* button : { &timeView, &spectrumView, &zoomAttack, &zoomFit, &zoomFull }) {
        targets.emplace_back(button->getComponentID(), button->getBounds());
    }
    for (bool horizontal : { true, false }) {
        const auto* bar = panel->hostedPanel().getZoomPanel()->getComponent(horizontal);
        const auto suffix = horizontal ? "X" : "Y";
        targets.emplace_back(juce::String("referencePitch.zoom") + suffix,
                getLocalArea(bar, bar->getLocalBounds()));
    }
    return targets;
}

juce::Rectangle<int> ReferencePitchEditor::cyclogramBounds() const {
    return getLocalArea(cyclogram.get(), cyclogram->plotBounds());
}

juce::Rectangle<int> ReferencePitchEditor::closeBounds() const {
    const auto* button = findChildWithID("referencePitch.close");
    return button != nullptr ? button->getBounds() : juce::Rectangle<int>();
}

void ReferencePitchEditor::paint(juce::Graphics& graphics) {
    chrome.paint(graphics);
}

void ReferencePitchEditor::paintOverChildren(juce::Graphics& graphics) {
    const auto pitch = curveBounds().reduced(6);
    EnvelopePurposeIconRenderer::paintMonochrome(graphics, EnvelopePurpose::Pitch,
            pitch.withSize(CanvasChromeMetrics::panelOverlayButtonSize,
                    CanvasChromeMetrics::panelOverlayButtonSize).toFloat());
}

void ReferencePitchEditor::resized() {
    chrome.resized();
    auto bounds = getLocalBounds().withTrimmedTop(CanvasChromeMetrics::fullEditorHeaderHeight)
            .reduced(12, 8);
    const int pitchHeight = juce::jlimit(90, 170, bounds.getHeight() / 3);
    auto surfaceBounds = bounds.removeFromTop(juce::jmax(0, bounds.getHeight() - pitchHeight - 6));
    bounds.removeFromTop(6);
    if (Component* component = host->zoomComponentIfCreated()) {
        component->setBounds(bounds);
    }
    surfaceBounds.setRight(curveBounds().getRight());
    cyclogram->setBounds(surfaceBounds);
    const int size = CanvasChromeMetrics::panelOverlayButtonSize;
    const int step = size + CanvasChromeMetrics::panelOverlayButtonGap;
    auto surface = cyclogramBounds().reduced(6);
    timeView.setBounds(surface.getX(), surface.getY(), size, size);
    spectrumView.setBounds(surface.getX() + step, surface.getY(), size, size);
    auto pitch = curveBounds().reduced(6);
    zoomAttack.setBounds(pitch.getRight() - 2 * step - size, pitch.getY(), size, size);
    zoomFit.setBounds(pitch.getRight() - step - size, pitch.getY(), size, size);
    zoomFull.setBounds(pitch.getRight() - size, pitch.getY(), size, size);
    for (auto* button : { &timeView, &spectrumView, &zoomAttack, &zoomFit, &zoomFull }) {
        button->toFront(false);
    }
    if (onRenderRequested) {
        onRenderRequested();
    }
}

void ReferencePitchEditor::scrollBarMoved(juce::ScrollBar*, double) {
    // Let ZoomPanel apply the scrollbar range before the GL thread reads it.
    juce::Component::SafePointer<ReferencePitchEditor> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis != nullptr) {
            safeThis->syncTimeZoom(false);
        }
    });
}

bool ReferencePitchEditor::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::spaceKey && onAuditionToggle) {
        onAuditionToggle();
        return true;
    }
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
    // Loading and interaction prepare the curve on the message thread. Rendering
    // must not reset scrollbars while a pending range change awaits delivery.
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

}
