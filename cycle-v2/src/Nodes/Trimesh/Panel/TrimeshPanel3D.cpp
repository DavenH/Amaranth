#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

#include "Graph/NodeGraph.h"
#include "UI/MeshEditorPresentation.h"

#include <App/AppConstants.h>
#include <UI/Panels/CommonGfx.h>
#include <UI/Panels/ZoomPanel.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>

#include <utility>
#include <vector>

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, Panel3D::DataRetriever& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true) {
    applyGradientForProfile();
    volumeScale = 1.f;
    volumeTrans = 0.f;
    guideCurveApplicable = true;
    speedApplicable = false;
    setInterceptPointScale(MeshEditorPresentation::interceptPointScale);
}

void TrimeshPanel3D::panelResized() {
    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    updateNameTexturePos();
    updateBackground();
    doExtraResized();
}

void TrimeshPanel3D::zoomUpdated(int updateSource) {
    Panel3D::zoomUpdated(updateSource);
    if (zoomChangedCallback != nullptr) {
        zoomChangedCallback(false);
    }
}

void TrimeshPanel3D::doZoomExtra(bool commandDown) {
    Panel3D::doZoomExtra(commandDown);
    if (zoomChangedCallback != nullptr) {
        zoomChangedCallback(true);
    }
}

void TrimeshPanel3D::setZoomChangedCallback(std::function<void(bool)> callback) {
    zoomChangedCallback = std::move(callback);
}

void TrimeshPanel3D::updateBackground(bool onlyVerticalBackground) {
    const bool spectral = renderProfile.getSliceStyle().isSpectral();
    Panel::updateBackground(onlyVerticalBackground || spectral);
}

void TrimeshPanel3D::drawBackground(bool fillBackground) {
    if (usesCachedSurface() && !shouldBakeTextures) {
        return;
    }
    ++backgroundPreparationCount;
    Panel::drawBackground(fillBackground);

    if (renderProfile.getSliceStyle().isSpectral() && gfx != nullptr) {
        drawSpectralHarmonics();
    }
}

void TrimeshPanel3D::drawSpectralHarmonics() {
    const int width = getWidth();
    if (width < 2 || getHeight() <= 0) {
        return;
    }

    const int firstNote = pitchSpansColumns
            ? Constants::LowestMidiNote : previewMidiNote;
    const Buffer<float> firstRamp = LogRegions::getDefaultRegion(firstNote);
    const int pointCount = pitchSpansColumns ? width : 2;
    const Range<int> midiRange(Constants::LowestMidiNote, Constants::HighestMidiNote);
    std::vector<Buffer<float>> pitchRamps;
    pitchRamps.reserve((size_t) pointCount);

    xBuffer.ensureSize(pointCount);
    yBuffer.ensureSize(pointCount);
    Buffer<float> x = xBuffer.withSize(pointCount);
    Buffer<float> y = yBuffer.withSize(pointCount);
    x.ramp(0.f, 1.f / (float) (pointCount - 1));
    for (int i = 0; i < pointCount; ++i) {
        const int midiNote = pitchSpansColumns
                ? Arithmetic::getGraphicNoteForValue(x[i], midiRange)
                : previewMidiNote;
        pitchRamps.push_back(LogRegions::getDefaultRegion(midiNote));
    }
    applyScaleX(x);

    gfx->disableSmoothing();
    gfx->setCurrentLineWidth(1.f);
    for (int harmonic = 8; harmonic < firstRamp.size(); harmonic += 8) {
        if (!drawHarmonicLine(harmonic, x, y, pitchRamps)) {
            break;
        }
    }
}

bool TrimeshPanel3D::drawHarmonicLine(
        int harmonic,
        Buffer<float> x,
        Buffer<float> y,
        const std::vector<Buffer<float>>& pitchRamps) {
    int validCount = 0;
    for (int i = 0; i < x.size(); ++i) {
        const Buffer<float> ramp = pitchRamps[(size_t) i];
        if (harmonic >= ramp.size()) {
            break;
        }
        y[i] = ramp[harmonic];
        ++validCount;
    }
    if (validCount < 2) {
        return false;
    }

    Buffer<float> scaledY = y.withSize(validCount);
    applyScaleY(scaledY);
    const bool major = harmonic % 32 == 0;
    const float brightness = major ? 0.15f : 0.095f;
    gfx->setCurrentColour(brightness, brightness, brightness);
    BufferXY line;
    line.x = x.withSize(validCount);
    line.y = scaledY;
    gfx->drawLineStrip(line, true, false);
    return true;
}

void TrimeshPanel3D::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshPanel3D::setPrimaryViewAxis(int axis) {
    if (primaryViewAxis != axis) {
        primaryViewAxis = axis;
        requestRepaint(PanelDirtyState::Flag::Overlay);
    }
}

Colour TrimeshPanel3D::primaryAxisColour() const {
    const auto dimension = primaryViewAxis == Vertex::Time ? MorphDimension::Yellow
            : primaryViewAxis == Vertex::Red ? MorphDimension::Red : MorphDimension::Blue;
    return colourForMorphDimension(dimension);
}

Rectangle<float> TrimeshPanel3D::primaryAxisBarBounds() const {
    auto bounds = getLocalBounds().toFloat().reduced(1.f);
    return primaryAxisVertical ? Rectangle<float>() : bounds.removeFromBottom(3.f);
}

void TrimeshPanel3D::appendPrimaryAxisAutomation(DynamicObject& state,
        std::optional<Rectangle<float>> barBounds) const {
    const auto bounds = barBounds.value_or(primaryAxisBarBounds());
    auto* rectangle = new DynamicObject();
    rectangle->setProperty("x", bounds.getX());
    rectangle->setProperty("y", bounds.getY());
    rectangle->setProperty("width", bounds.getWidth());
    rectangle->setProperty("height", bounds.getHeight());
    playbackOverlay.appendAutomation(state, primaryViewAxis, primaryAxisVertical, true);
    state.setProperty("backgroundPreparationCount", (int64) backgroundPreparationCount.load());
    state.setProperty("guideTablePreparationCount", (int64) guideTablePreparationCount.load());
    state.setProperty("surfaceDrawCount", (int64) surfaceDrawCount.load());
    state.setProperty("surfaceDrawMicroseconds", (int64) surfaceDrawMicroseconds.load());
    state.setProperty("primaryAxisBarBounds", rectangle);
    state.setProperty("primaryAxisColour", primaryAxisColour().toString());
    state.setProperty("primaryAxis", primaryAxisVertical ? "y" : "x");
    state.setProperty("primaryViewAxis", primaryViewAxis == Vertex::Time ? "yellow"
            : primaryViewAxis == Vertex::Red ? "red" : "blue");
}

void TrimeshPanel3D::setPlaybackCursors(const PlaybackCursorFrame& frame) {
    const ScopedLock lock(getRenderLock());
    if (playbackOverlay.setFrame(frame)) {
        requestRepaint(PanelDirtyState::Flag::Overlay);
    }
}

void TrimeshPanel3D::setKeyScaleAxis(int axis) {
    const ScopedLock lock(getRenderLock());
    if (playbackOverlay.setKeyScaleAxis(axis)) {
        requestRepaint(PanelDirtyState::Flag::Overlay);
    }
}

void TrimeshPanel3D::setSlicePosition(float position) {
    const ScopedLock lock(getRenderLock());
    if (playbackOverlay.setSlice(position)) {
        requestRepaint(PanelDirtyState::Flag::Overlay);
    }
}

void TrimeshPanel3D::drawSurface() {
    const double started = Time::getMillisecondCounterHiRes();
    Panel3D::drawSurface();
    ++surfaceDrawCount;
    surfaceDrawMicroseconds += (uint64_t) ((Time::getMillisecondCounterHiRes() - started) * 1000.0);
}

void TrimeshPanel3D::postVertsDraw() {
    playbackOverlay.draw(*this, primaryViewAxis, primaryAxisVertical, true);
    if (auto* renderer = getPanelRenderer()) {
        const auto bounds = primaryAxisBarBounds();
        if (bounds.isEmpty()) {
            return;
        }
        const Colour colour = primaryAxisColour();
        renderer->setCurrentColour(colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), 1.f);
        renderer->fillRect(bounds.getX(), bounds.getY(), bounds.getRight(), bounds.getBottom(), false);
    }
}

bool TrimeshPanel3D::getScalarSurfaceMaterial(ScalarSurfaceMaterial& material) const {
    if (renderProfile.getDomain() == PortDomain::SpectralMagnitudeSignal) {
        return false;
    }

    material = renderProfile.getSurfaceStyle().surfaceMaterial();
    return true;
}

Buffer<Float32> TrimeshPanel3D::getLineGuideTable(int channel) {
    if (isPositiveAndBelow(channel, (int) lineGuideTables.size())) {
        auto& table = lineGuideTables[(size_t) channel];
        return { table.data(), (int) table.size() };
    }
    return Panel3D::getLineGuideTable(channel);
}

void TrimeshPanel3D::setLineGuideTables(std::vector<std::vector<float>> tables) {
    const ScopedLock lock(getRenderLock());
    lineGuideTables = std::move(tables);
    ++guideTablePreparationCount;
    requestRepaint();
}

void TrimeshPanel3D::setRenderProfile(TrimeshRenderProfile profile) {
    const bool domainChanged = profile.getDomain() != renderProfile.getDomain()
            || profile.getScalePolicy() != renderProfile.getScalePolicy();
    if (!domainChanged
            && profile.getSurfaceStyle().materialStyleSignature()
                    == appliedMaterialStyleSignature) {
        return;
    }

    renderProfile = profile;
    haveLogarithmicY = profile.getDomain() == PortDomain::SpectralMagnitudeSignal
            || profile.getDomain() == PortDomain::SpectralPhaseSignal;
    applyGradientForProfile();
    if (domainChanged) {
        dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    }
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    if (getComponent() != nullptr) {
        updateBackground();
        requestRepaint();
    }
}

void TrimeshPanel3D::configureViewport(Range<float> frequencyLimits) {
    if (getZoomPanel() == nullptr) {
        return;
    }

    ZoomRect& rect = getZoomPanel()->rect;
    if (rect.yMinimum == frequencyLimits.getStart()
            && rect.yMaximum == frequencyLimits.getEnd()) {
        return;
    }
    rect.yMinimum = frequencyLimits.getStart();
    rect.yMaximum = frequencyLimits.getEnd();
    rect.y = 0.f;
    rect.h = 1.f;
    getZoomPanel()->panelZoomChanged(false);
}

void TrimeshPanel3D::setPitchSpansColumns(bool shouldSpan) {
    if (pitchSpansColumns == shouldSpan) {
        return;
    }

    pitchSpansColumns = shouldSpan;
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    requestRepaint();
}

void TrimeshPanel3D::setPreviewMidiNote(int midiNote) {
    if (previewMidiNote == midiNote) {
        return;
    }

    previewMidiNote = midiNote;
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    requestRepaint();
}

void TrimeshPanel3D::applyGradientForProfile() {
    const auto& surfaceStyle = renderProfile.getSurfaceStyle();
    Image image = surfaceStyle.gradientImage();

    appliedMaterialStyleSignature = surfaceStyle.materialStyleSignature();
    isTransparent = surfaceStyle.textureUsesAlpha;
    gradient.read(image, true, isTransparent);
}

}
