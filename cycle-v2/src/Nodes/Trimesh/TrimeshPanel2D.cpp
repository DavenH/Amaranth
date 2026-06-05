#include "TrimeshPanel2D.h"

#include <UI/Panels/CommonGfx.h>
#include <Util/Arithmetic.h>

namespace CycleV2 {

namespace {

constexpr float kSpectrumTension = 500.f;

}

TrimeshPanel2D::TrimeshPanel2D(SingletonRepo* repo) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel2D")
    ,   Panel2D            (repo, "CycleV2TrimeshPanel2D", true, true) {
    guideCurveApplicable = false;
    speedApplicable = false;
    pendingDeformUpdate = false;
    backgroundTimeRelevant = false;
    applyRenderProfile();
}

void TrimeshPanel2D::drawBackground(bool fillBackground) {
    if (renderProfile.getSliceBackground() == TrimeshSliceBackground::Spectrum) {
        drawSpectrumBackground(fillBackground);
        return;
    }

    drawWaveformBackground(fillBackground);
}

void TrimeshPanel2D::panelResized() {
    dirtyState.mark(PanelDirtyState::Flag::Layout);
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    updateNameTexturePos();
    updateBackground();
    doExtraResized();
}

void TrimeshPanel2D::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshPanel2D::setRenderProfile(TrimeshRenderProfile profile) {
    if (profile.getDomain() == renderProfile.getDomain()
            && profile.getScalePolicy() == renderProfile.getScalePolicy()) {
        return;
    }

    renderProfile = profile;
    applyRenderProfile();
    backgroundTimeRelevant = false;
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);

    if (getComponent() == nullptr) {
        return;
    }

    updateBackground();
    requestRepaint();
}

void TrimeshPanel2D::applyRenderProfile() {
    setCurveBipolar(renderProfile.curveIsBipolar());
    setColors(renderProfile.negativeCurveColour(), renderProfile.positiveCurveColour());
}

void TrimeshPanel2D::drawWaveformBackground(bool fillBackground) {
    minorBrightness = 0.085f;
    majorBrightness = 0.14f;
    Panel::drawBackground(fillBackground);

    if (gfx == nullptr) {
        return;
    }

    gfx->setCurrentColour(0.28f, 0.28f, 0.32f, 0.18f);
    gfx->fillRect(0, 0, 1.f, 0.25f, true);
    gfx->fillRect(0, 0.75f, 1.f, 1.f, true);
}

void TrimeshPanel2D::drawSpectrumBackground(bool fillBackground) {
    if (gfx == nullptr) {
        return;
    }

    const int width = getWidth();
    const int height = getHeight();

    if (width <= 0 || height <= 0) {
        return;
    }

    if (fillBackground) {
        gfx->setCurrentColour(0.055f, 0.045f, 0.04f);
        gfx->fillRect(0, 0, width, height, false);
    }

    gfx->disableSmoothing();
    gfx->setCurrentLineWidth(1.f);

    xBuffer.ensureSize(128);
    Buffer<float> scaledRamp = xBuffer.withSize(128);
    scaledRamp.ramp(0.f, 1.f / (float) scaledRamp.size());
    Arithmetic::applyLogMapping(scaledRamp, kSpectrumTension);
    applyScaleX(scaledRamp);

    for (int i = 0; i < scaledRamp.size() - 7; i += 4) {
        const float progress = (float) i / (float) scaledRamp.size();
        const float base = i % 16 == 0 ? 0.165f : 0.14f;
        const Color c1(base - 0.08f * progress);
        const Color c2(0.07f - 0.01f * progress);

        gfx->drawLine(scaledRamp[i], sy(0.f), scaledRamp[i], sy(1.f), c1, c2);
    }

    yBuffer.ensureSize(24);
    cBuffer.ensureSize(24);
    Buffer<float> dbLines = yBuffer.withSize(24);
    Buffer<float> progress = cBuffer.withSize(24);

    float value = 1.f;
    for (int i = 0; i < dbLines.size(); ++i) {
        dbLines[i] = value;
        value *= 0.5f;
    }

    Arithmetic::applyLogMapping(dbLines, kSpectrumTension);
    applyScaleY(dbLines);
    progress.ramp(1.f, -1.f / (float) progress.size());

    for (int i = 0; i < dbLines.size(); ++i) {
        const float p = progress[i];
        const Color c1(0.16f - 0.06f * p);
        const Color c2(0.07f - 0.005f * p);
        gfx->drawLine(sx(0.f), dbLines[i], sx(1.f), dbLines[i], c1, c2);
    }

    gfx->setCurrentColour(0.22f, 0.22f, 0.22f, 0.34f);
    gfx->drawLine(0.f, sy(0.f), (float) width, sy(0.f), false);
    gfx->drawLine(0.f, sy(1.f), (float) width, sy(1.f), false);
}

}
