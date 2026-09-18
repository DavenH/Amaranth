#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

#include "UI/MeshEditorPresentation.h"

#include <App/AppConstants.h>
#include <UI/Panels/CommonGfx.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>

#include <vector>

namespace CycleV2 {

TrimeshPanel3D::TrimeshPanel3D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel3D")
    ,   Panel3D            (repo, "CycleV2TrimeshPanel3D", &source, 0, false, true)
    ,   dataSource         (source) {
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

void TrimeshPanel3D::updateBackground(bool onlyVerticalBackground) {
    const bool spectral = renderProfile.getSliceStyle().isSpectral();
    Panel::updateBackground(onlyVerticalBackground || spectral);
}

void TrimeshPanel3D::drawBackground(bool fillBackground) {
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

void TrimeshPanel3D::setRenderProfile(TrimeshRenderProfile profile) {
    if (profile.getDomain() == renderProfile.getDomain()
            && profile.getScalePolicy() == renderProfile.getScalePolicy()) {
        return;
    }

    renderProfile = profile;
    haveLogarithmicY = profile.getDomain() == PortDomain::SpectralMagnitudeSignal
            || profile.getDomain() == PortDomain::SpectralPhaseSignal;
    applyGradientForProfile();
    dirtyState.mark(PanelDirtyState::Flag::SurfaceCache);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    if (getComponent() != nullptr) {
        updateBackground();
        requestRepaint();
    }
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

    isTransparent = surfaceStyle.textureUsesAlpha;
    gradient.read(image, true, isTransparent);
}

}
