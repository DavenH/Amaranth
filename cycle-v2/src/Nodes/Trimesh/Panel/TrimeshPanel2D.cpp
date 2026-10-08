#include "Nodes/Trimesh/Panel/TrimeshPanel2D.h"

#include <UI/Panels/CommonGfx.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>
#include <Util/LogRegions.h>

#include "Nodes/Trimesh/Panel/TrimeshPanelDataSource.h"
#include "UI/MeshEditorPresentation.h"

namespace CycleV2 {

TrimeshPanel2D::TrimeshPanel2D(SingletonRepo* repo, TrimeshPanelDataSource& source) :
        SingletonAccessor  (repo, "CycleV2TrimeshPanel2D")
    ,   Panel2D            (repo, "CycleV2TrimeshPanel2D", true, true)
    ,   dataSource         (source) {
    guideCurveApplicable = true;
    speedApplicable = false;
    backgroundTimeRelevant = false;
    setInterceptPointScale(MeshEditorPresentation::interceptPointScale);
    applyRenderProfile();
}

void TrimeshPanel2D::preDraw() {
    if (renderProfile.getSliceStyle().isSpectral()) {
        drawSpectralPartials();
    }
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

void TrimeshPanel2D::setPreviewMidiNote(int midiNote) {
    if (previewMidiNote == midiNote) {
        return;
    }

    previewMidiNote = midiNote;
    dirtyState.mark(PanelDirtyState::Flag::StaticVisual);
    if (getComponent() != nullptr) {
        updateBackground();
        requestRepaint();
    }
}

void TrimeshPanel2D::setPrimaryMorphPosition(float position) {
    if (primaryMorphPosition == position) {
        return;
    }

    primaryMorphPosition = position;
    requestRepaint();
}

void TrimeshPanel2D::applyRenderProfile() {
    const auto& curveStyle = renderProfile.getCurveStyle();

    setCurveBipolar(curveStyle.bipolar);
    // Panel2D uses its first colour above the centre line.
    setColors(curveStyle.positiveColour, curveStyle.negativeColour);
}

void TrimeshPanel2D::drawSpectralPartials() {
    if (gfx == nullptr || getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    int midiNote {};
    if (!dataSource.copyColumnAtMorph(primaryMorphPosition, partialValues, midiNote)) {
        return;
    }

    const Buffer<float> ramp = LogRegions::getDefaultRegion(midiNote);
    const int count = jmin((int) partialValues.size(), ramp.size());
    if (count < 2) {
        return;
    }

    xBuffer.ensureSize(count);
    yBuffer.ensureSize(count);
    Buffer<float> positions = xBuffer.withSize(count);
    Buffer<float> heights = yBuffer.withSize(count);
    ramp.withSize(count).copyTo(positions);
    Buffer<float>(partialValues.data(), count).copyTo(heights);
    applyScaleX(positions);
    applyScaleY(heights);

    const float baseline = sy(renderProfile.getCurveStyle().bipolar ? 0.5f : 0.f);
    const Color body(0.72f, 0.75f, 0.82f, 0.20f);
    const Color cap(0.88f, 0.90f, 0.94f, 0.30f);
    partialContour.clear();
    partialContour.reserve((size_t) count);

    for (int i = 0; i < count - 1; ++i) {
        const float spacing = positions[i + 1] - positions[i];
        if (spacing < 8.f || !partialContour.empty()) {
            ColorPos point;
            point.x = positions[i];
            point.y = heights[i];
            point.c = body;
            partialContour.push_back(point);
            continue;
        }

        const float inset = spacing * 0.2f;
        const float left = positions[i] + inset;
        const float right = positions[i + 1] - inset;
        const float top = jmin(heights[i], baseline);
        const float bottom = jmax(heights[i], baseline);
        if (right <= left || bottom - top < 1.f) {
            continue;
        }

        gfx->setCurrentColour(body);
        if (spacing >= 12.f && bottom - top >= 8.f) {
            const float corner = jmin(3.f, jmin(
                    (right - left) * 0.25f,
                    (bottom - top) * 0.25f));
            if (heights[i] < baseline) {
                gfx->fillRect(left + corner, top, right - corner, top + corner * 0.5f, false);
                gfx->fillRect(left + corner * 0.4f, top + corner * 0.5f,
                        right - corner * 0.4f, top + corner, false);
                gfx->fillRect(left, top + corner, right, bottom, false);
            } else {
                gfx->fillRect(left, top, right, bottom - corner, false);
                gfx->fillRect(left + corner * 0.4f, bottom - corner,
                        right - corner * 0.4f, bottom - corner * 0.5f, false);
                gfx->fillRect(left + corner, bottom - corner * 0.5f,
                        right - corner, bottom, false);
            }
        } else {
            gfx->fillRect(left, top, right, bottom, false);
        }

        if (spacing >= 12.f) {
            gfx->setCurrentColour(cap);
            gfx->drawLine(left + 2.f, heights[i], right - 2.f, heights[i], false);
        }
    }

    if (!partialContour.empty()) {
        ColorPos end;
        end.x = positions[count - 1];
        end.y = heights[count - 1];
        end.c = body;
        partialContour.push_back(end);
        gfx->fillAndOutlineColoured(partialContour, baseline, 0.08f, true, false);
    }
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

    const Buffer<float> ramp = LogRegions::getDefaultRegion(previewMidiNote);
    xBuffer.ensureSize(ramp.size());
    Buffer<float> scaledRamp = xBuffer.withSize(ramp.size());
    ramp.copyTo(scaledRamp);
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

    Arithmetic::applyLogMapping(dbLines, 500.f);
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

    const Buffer<float> ramp = LogRegions::getDefaultRegion(previewMidiNote);
    xBuffer.ensureSize(ramp.size());
    Buffer<float> scaledRamp = xBuffer.withSize(ramp.size());
    ramp.copyTo(scaledRamp);
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
