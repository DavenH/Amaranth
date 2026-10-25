#include <utility>

#include "UI/SignalProbeDetailView.h"

#include "Runtime/PreviewTraversal.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UiIconData.h"

namespace CycleV2 {

namespace {

const Colour kBackdrop { 0xb0000000 };
constexpr float kTimelineHeight = 30.f;

const Drawable* envelopeIcon(bool release) {
    const auto load = [](const char* source) {
        const auto document = parseXML(source);
        jassert(document != nullptr);
        return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
    };
    static const auto normal = load(UiIconData::spyEnvelopeNormal);
    static const auto tail = load(UiIconData::spyEnvelopeRelease);
    return release ? tail.get() : normal.get();
}

void paintTimeline(Graphics& graphics, Rectangle<float> plot, const SignalProbeDetailState& state) {
    const float noteOff = state.noteOffPosition();
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float split = content.getX() + content.getWidth() * noteOff;
    const auto paintRegion = [&](float left, float right, bool release) {
        // Wide envelope diagram: 36x24 canvas in a 36x30 annotation area.
        const float centre = jlimit(content.getX() + 18.f, content.getRight() - 18.f, (left + right) * 0.5f);
        if (const Drawable* icon = envelopeIcon(release)) {
            icon->drawWithin(graphics,
                    Rectangle<float>(36.f, 24.f).withCentre({ centre, plot.getY() - kTimelineHeight * 0.5f }),
                    RectanglePlacement::centred, 1.f);
        }
        graphics.setColour(CanvasChromePalette::border);
        graphics.drawHorizontalLine(roundToInt(plot.getY() - 3.f), left, right);
    };
    paintRegion(content.getX(), split, false);
    if (noteOff < 1.f) {
        paintRegion(split, content.getRight(), true);
        graphics.setColour(CanvasChromePalette::strongBorder);
        graphics.drawVerticalLine(roundToInt(split), plot.getY() - 7.f, plot.getY());
    }
}

}

float SignalProbeDetailState::noteOffPosition() const {
    return PreviewTraversal(renderResult.gridColumns, releaseDuration).noteOffPosition();
}

void SignalProbeDetailState::open(
        GraphPreviewResult::SignalProbePreview previewToUse,
        RenderScalePolicy scalePolicyToUse,
        int ordinalToUse,
        int midiNoteToUse,
        size_t resolutionToUse) {
    probeId = previewToUse.probeId;
    domain = previewToUse.domain;
    scalePolicy = scalePolicyToUse;
    ordinal = ordinalToUse;
    midiNote = midiNoteToUse;
    resolution = resolutionToUse;
    releaseDuration = previewToUse.releaseDuration;
    renderResult = {
            "probe-detail-" + probeId,
            PreviewModuleRole::SignalSpy,
            std::move(previewToUse.values),
            {},
            previewToUse.gridColumns,
            previewToUse.gridRows,
            domain,
            previewToUse.frequencySampling,
            previewToUse.frequencyMidiNote
    };
}

Rectangle<float> SignalProbeDetailView::boundsFor(Rectangle<float> availableContent) {
    const float width = jmin(920.f, availableContent.getWidth() * 0.82f);
    const float height = jmin(620.f, availableContent.getHeight() * 0.78f);
    return Rectangle<float>(width, height).withCentre(availableContent.getCentre());
}

Rectangle<float> SignalProbeDetailView::plotBounds(Rectangle<float> detailBounds) {
    return detailBounds.reduced(14.f).withTrimmedTop(kTimelineHeight);
}

bool SignalProbeDetailView::dismissesOnClick(
        Rectangle<float> detailBounds,
        Point<float> position,
        int clickCount) {
    return clickCount >= 2 && detailBounds.contains(position);
}

void SignalProbeDetailView::paint(
        Graphics& graphics,
        Rectangle<float> availableContent,
        const SignalProbeDetailState& state) {
    if (!state.isOpen()) {
        return;
    }

    graphics.setColour(kBackdrop);
    graphics.fillRect(availableContent);

    const Rectangle<float> detail = boundsFor(availableContent);
    graphics.setColour(CanvasChromePalette::insetBackground);
    graphics.fillRoundedRectangle(detail, CanvasChromeMetrics::panelCornerRadius);
    graphics.setColour(CanvasChromePalette::border);
    graphics.drawRoundedRectangle(
            detail,
            CanvasChromeMetrics::panelCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);

    const Rectangle<float> plot = plotBounds(detail);
    paintTimeline(graphics, plot, state);

    Node displayNode;
    displayNode.id = state.renderResult.nodeId;
    displayNode.kind = NodeKind::GenericProcessor;
    renderer.paint(graphics, {
            displayNode,
            &state.renderResult,
            plot,
            TrimeshRenderProfile::fromSemantic({
                    state.domain,
                    state.scalePolicy,
                    RenderSemanticRole::Generic
            }),
            1.f,
            true,
            {},
            true
    });
}

}
