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
    const auto& sliceStyle = renderProfile.getSliceStyle();

    if (sliceStyle.background == TrimeshSliceBackground::SpectrumMagnitude) {
        drawSpectrumMagnitudeBackground(fillBackground);
        return;
    }

    if (sliceStyle.background == TrimeshSliceBackground::SpectrumPhase) {
        drawSpectrumPhaseBackground(fillBackground);
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
    const auto& curveStyle = renderProfile.getCurveStyle();

    setCurveBipolar(curveStyle.bipolar);
    setColors(curveStyle.negativeColour, curveStyle.positiveColour);
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

void TrimeshPanel2D::drawSpectrumMagnitudeBackground(bool fillBackground) {
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

void TrimeshPanel2D::drawSpectrumPhaseBackground(bool fillBackground) {
    if (gfx == nullptr) {
        return;
    }

    const int width = getWidth();
    const int height = getHeight();

    if (width <= 0 || height <= 0) {
        return;
    }

    if (fillBackground) {
        gfx->setCurrentColour(0.048f, 0.044f, 0.060f);
        gfx->fillRect(0, 0, width, height, false);
    }

    gfx->disableSmoothing();
    gfx->setCurrentLineWidth(1.f);

    xBuffer.ensureSize(128);
    Buffer<float> scaledRamp = xBuffer.withSize(128);
    scaledRamp.ramp(0.f, 1.f / (float) scaledRamp.size());
    Arithmetic::applyLogMapping(scaledRamp, kSpectrumTension);
    applyScaleX(scaledRamp);

    const int quarterSize = scaledRamp.size() / 4;
    yBuffer.ensureSize(quarterSize);
    cBuffer.ensureSize(quarterSize);

    Buffer<float> rampReduced = yBuffer.withSize(quarterSize);
    Buffer<float> phaseLines = cBuffer.withSize(quarterSize);
    BufferXY phaseXY;
    phaseXY.x = rampReduced;
    phaseXY.y = phaseLines;

    rampReduced.downsampleFrom(scaledRamp, 4);

    gfx->setCurrentColour(0.075f, 0.068f, 0.095f);
    for (float x : rampReduced) {
        gfx->drawLine(x, sy(0.f), x, sy(1.f), false);
    }

    Buffer<float> phaseShape = xBuffer.withSize(quarterSize);
    phaseShape.ramp(1.f, 1.f).sqrt().divCRev(0.5f).add(0.5f);

    gfx->setCurrentLineWidth(1.f);

    for (int i = 0; i < 8; ++i) {
        phaseShape.copyTo(phaseLines);
        applyScaleY(phaseLines);
        gfx->setCurrentColour(i == 7 ? 0.145f : 0.105f, 0.095f, i == 7 ? 0.185f : 0.145f);
        gfx->drawLineStrip(phaseXY, true, false);

        phaseShape.subCRev(1.f).copyTo(phaseLines);
        applyScaleY(phaseLines);
        gfx->drawLineStrip(phaseXY, true, false);

        const float scale = (float) (i + 2) / (float) (i + 1);
        phaseShape.add(-0.5f).mul(scale).add(0.5f);
    }

    gfx->setCurrentColour(0.70f, 0.52f, 1.f, 0.18f);
    gfx->drawLine(0.f, sy(0.5f), (float) width, sy(0.5f), false);
}

}
