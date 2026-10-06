#include <utility>

#include "UI/SignalProbeDetailView.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

const Colour kBackdrop { 0xb0000000 };

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

size_t SignalProbeDetailView::resolutionForMidiNote(int midiNote, double sampleRate) {
    return GraphPreviewExecutor::periodRowsForMidiNote(midiNote, sampleRate);
}

Rectangle<float> SignalProbeDetailView::boundsFor(Rectangle<float> availableContent) {
    const float width = jmin(920.f, availableContent.getWidth() * 0.82f);
    const float height = jmin(620.f, availableContent.getHeight() * 0.78f);
    return Rectangle<float>(width, height).withCentre(availableContent.getCentre());
}

Rectangle<float> SignalProbeDetailView::plotBounds(Rectangle<float> detailBounds) {
    return detailBounds.reduced(14.f);
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

    Node displayNode;
    displayNode.id = state.renderResult.nodeId;
    displayNode.kind = NodeKind::GenericProcessor;
    renderer.paint(graphics, {
            displayNode,
            &state.renderResult,
            plotBounds(detail),
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
