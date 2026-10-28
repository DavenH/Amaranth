#include <cmath>
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

Rectangle<float> iconBounds(Rectangle<float> plot, float left, float right, bool release) {
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float centre = jlimit(content.getX() + 18.f, content.getRight() - 18.f,
            release ? left : (left + right) * 0.5f);
    return Rectangle<float>(36.f, 24.f).withCentre({ centre, plot.getY() - kTimelineHeight * 0.5f });
}

void paintTimeline(Graphics& graphics, Rectangle<float> plot, const SignalProbeDetailState& state) {
    const float noteOff = state.noteOffPosition();
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float split = content.getX() + content.getWidth() * noteOff;
    const auto paintRegion = [&](float left, float right, bool release) {
        if (const Drawable* icon = envelopeIcon(release)) {
            icon->drawWithin(graphics,
                    iconBounds(plot, left, right, release),
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

double SignalProbeDetailState::displayedNoteOffTime() const {
    return releaseDragging ? dragTime : noteOffTime;
}

float SignalProbeDetailState::noteOffPosition() const {
    return PreviewTraversal(renderResult.gridColumns, releaseDuration, displayedNoteOffTime()).noteOffPosition();
}

bool SignalProbeDetailState::releaseHitTest(Rectangle<float> detail, Point<float> position) const {
    return isOpen() && (SignalProbeDetailView::releaseHandleBounds(detail, *this).contains(position)
            || SignalProbeDetailView::releaseIconBounds(detail, *this).contains(position));
}

bool SignalProbeDetailState::beginReleaseDrag(Rectangle<float> detail, Point<float> position) {
    if (!releaseHitTest(detail, position)) {
        return false;
    }
    releaseDragging = true;
    dragStartTime = noteOffTime;
    dragTime = noteOffTime;
    dragStartX = position.x;
    dragVoiceWidth = jmax(1.f, SignalProbeDetailView::voiceBounds(detail, *this).getWidth());
    return true;
}

void SignalProbeDetailState::dragRelease(Point<float> position) {
    if (releaseDragging) {
        dragTime = jlimit(0., 1., dragStartTime + (double) ((position.x - dragStartX) / dragVoiceWidth));
    }
}

bool SignalProbeDetailState::endReleaseDrag(Point<float> position) {
    if (!releaseDragging) {
        return false;
    }
    dragRelease(position);
    releaseDragging = false;
    if (std::abs(dragTime - noteOffTime) < 1.0e-6) {
        return false;
    }
    noteOffTime = dragTime;
    return true;
}

bool SignalProbeDetailState::cancelReleaseDrag() {
    const bool wasDragging = releaseDragging;
    releaseDragging = false;
    return wasDragging;
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
    noteOffTime = previewToUse.noteOffTime;
    releaseDragging = false;
    ++renderCount;
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
    renderResult.contentRevision = nodePreviewResultFingerprint(renderResult);
}

Rectangle<float> SignalProbeDetailView::boundsFor(Rectangle<float> availableContent) {
    const float width = jmin(920.f, availableContent.getWidth() * 0.82f);
    const float height = jmin(620.f, availableContent.getHeight() * 0.78f);
    return Rectangle<float>(width, height).withCentre(availableContent.getCentre());
}

Rectangle<float> SignalProbeDetailView::plotBounds(Rectangle<float> detailBounds) {
    return detailBounds.reduced(14.f).withTrimmedTop(kTimelineHeight);
}

Rectangle<float> SignalProbeDetailView::voiceBounds(
        Rectangle<float> detail, const SignalProbeDetailState& state) {
    const auto plot = plotBounds(detail);
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float end = PreviewTraversal(state.renderResult.gridColumns, state.releaseDuration).noteOffPosition();
    return Rectangle<float>(content.getX(), plot.getY() - kTimelineHeight,
            content.getWidth() * end, content.getBottom() - plot.getY() + kTimelineHeight);
}

Rectangle<float> SignalProbeDetailView::releaseHandleBounds(
        Rectangle<float> detail, const SignalProbeDetailState& state) {
    const auto plot = plotBounds(detail);
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float split = content.getX() + content.getWidth() * state.noteOffPosition();
    return { split - 7.f, plot.getY() - kTimelineHeight, 14.f,
            content.getBottom() - plot.getY() + kTimelineHeight };
}

Rectangle<float> SignalProbeDetailView::releaseIconBounds(
        Rectangle<float> detail, const SignalProbeDetailState& state) {
    if (state.noteOffPosition() >= 1.f) {
        return {};
    }
    const auto plot = plotBounds(detail);
    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    return iconBounds(plot, content.getX() + content.getWidth() * state.noteOffPosition(),
            content.getRight(), true);
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
        Rectangle<float> detail,
        const SignalProbeDetailState& state) {
    if (!state.isOpen()) {
        return;
    }

    graphics.setColour(kBackdrop);
    graphics.fillRect(availableContent);

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

    const auto content = NodePreviewRenderer::heatmapBounds(plot);
    const float split = releaseHandleBounds(detail, state).getCentreX();
    graphics.setColour(CanvasChromePalette::focus.withAlpha(state.releaseDragging ? 0.85f : 0.5f));
    graphics.drawVerticalLine(roundToInt(split), plot.getY() - 3.f, content.getBottom());
    Path handle;
    handle.addTriangle(split - 5.f, plot.getY() - 9.f,
            split + 5.f, plot.getY() - 9.f, split, plot.getY() - 3.f);
    graphics.setColour(CanvasChromePalette::focus);
    graphics.fillPath(handle);
}

}
