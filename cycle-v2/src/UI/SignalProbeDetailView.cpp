#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Util/Arithmetic.h>

#include <utility>

#include "UI/SignalProbeDetailView.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

namespace {

const Colour kBackdrop { 0xb0000000 };

void paintHeader(
        Graphics& graphics,
        Rectangle<float> detail,
        Rectangle<float> header,
        const SignalProbeDetailState& state) {
    graphics.setColour(colourForDomain(state.domain));
    graphics.fillEllipse(Rectangle<float>(10.f, 10.f).withCentre({
            header.getX() + 7.f,
            header.getCentreY()
    }));
    graphics.setColour(CanvasChromePalette::text);
    graphics.setFont(FontOptions(CanvasChromeMetrics::sectionTitleFontSize));
    graphics.drawText(
            String(state.ordinal),
            header.withTrimmedLeft(20.f),
            Justification::centredLeft);

    graphics.setColour(CanvasChromePalette::mutedText);
    graphics.setFont(FontOptions(CanvasChromeMetrics::captionFontSize));
    graphics.drawText(
            String((int) state.resolution) + " samples",
            header.withTrimmedRight(38.f),
            Justification::centredRight);

    const Rectangle<float> close = SignalProbeDetailView::closeBounds(detail).reduced(7.f);
    graphics.drawLine(Line<float>(close.getTopLeft(), close.getBottomRight()), 1.5f);
    graphics.drawLine(Line<float>(close.getTopRight(), close.getBottomLeft()), 1.5f);
}

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
    const PreviewModuleRole displayRole = previewToUse.sourceRole == PreviewModuleRole::MeshSurface
            ? PreviewModuleRole::MeshSurface
            : PreviewModuleRole::SignalSpy;
    renderResult = {
            "probe-detail-" + probeId,
            displayRole,
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
    const double angleDelta = CycleDsp::OscillatorLaneCore::angleDelta(
            jlimit(0, 127, midiNote),
            0.f,
            sampleRate);
    if (angleDelta <= 0.0) {
        return 0;
    }

    return (size_t) Arithmetic::getNextPow2((float) (1.0 / angleDelta));
}

Rectangle<float> SignalProbeDetailView::boundsFor(Rectangle<float> availableContent) {
    const float width = jmin(920.f, availableContent.getWidth() * 0.82f);
    const float height = jmin(620.f, availableContent.getHeight() * 0.78f);
    return Rectangle<float>(width, height).withCentre(availableContent.getCentre());
}

Rectangle<float> SignalProbeDetailView::closeBounds(Rectangle<float> detailBounds) {
    return Rectangle<float>(30.f, 30.f).withCentre({
            detailBounds.getRight() - 22.f,
            detailBounds.getY() + 22.f
    });
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

    Rectangle<float> content = detail.reduced(14.f);
    Rectangle<float> header = content.removeFromTop(34.f);
    paintHeader(graphics, detail, header, state);

    Node displayNode;
    displayNode.id = state.renderResult.nodeId;
    displayNode.kind = NodeKind::GenericProcessor;
    renderer.paint(graphics, {
            displayNode,
            &state.renderResult,
            content.withTrimmedTop(4.f),
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
