#include "UI/Editors/PropertyControls.h"
#include "UI/VoiceContextCompactEditor.h"

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "UI/CanvasChromeMetrics.h"
#include "UI/EnvelopePurposeIconRenderer.h"
#include "UI/NodePortGeometry.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

bool portamentoEnabled(const Node& node) {
    return parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";
}

String durationText(double voiceDurationSeconds) {
    const double duration = jmax(0.0, voiceDurationSeconds);
    const String value = formatPropertyReal(duration);
    return value + (approximatelyEqual(duration, 1.0) ? " second" : " seconds");
}

}

Rectangle<float> VoiceContextCompactEditor::summaryBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    return nodeBounds
            .withTrimmedTop(94.f * zoom)
            .withTrimmedBottom(42.f * zoom)
            .reduced(16.f * zoom, 0.f);
}

Rectangle<float> VoiceContextCompactEditor::scratchIndicatorBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    const float size = 18.f * zoom;
    const float centreY = nodeBounds.getY()
            + (NodePortGeometry::firstSidePortOffset
                    + 3.f * NodePortGeometry::sidePortSpacing) * zoom;
    return Rectangle<float>(size, size).withCentre({
            nodeBounds.getX() + 16.f * zoom,
            centreY
    });
}

Rectangle<float> VoiceContextCompactEditor::scratchLabelBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    const Rectangle<float> icon = scratchIndicatorBounds(nodeBounds, zoom);
    return Rectangle<float>(76.f * zoom, 24.f * zoom).withCentre({
            icon.getRight() + 43.f * zoom,
            icon.getCentreY()
    });
}

String VoiceContextCompactEditor::summaryLabel(const Node& node) {
    const String octave = parameterValueForNode(node, "octave", "0");
    const double voiceDurationSeconds = CycleDsp::voiceLengthSeconds(
            parameterValueForNode(
                    node,
                    "voiceLength",
                    String(CycleDsp::voiceLengthUnitValue(1.0))).getFloatValue());
    String summary = "Octave " + octave + "  ·  " + durationText(voiceDurationSeconds);
    if (portamentoEnabled(node)) {
        summary += "  ·  Glide";
    }
    return summary;
}

void VoiceContextCompactEditor::paintNodeSummary(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom,
        const Node& node) {
    const Rectangle<float> summary = summaryBounds(nodeBounds, zoom);
    graphics.setColour(kText.withAlpha(0.88f));
    graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize * zoom));
    graphics.drawText(
            summaryLabel(node),
            summary,
            Justification::centred);
}

void VoiceContextCompactEditor::paintScratchIndicator(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom) {
    EnvelopePurposeIconRenderer::paint(
            graphics,
            EnvelopePurpose::Scratch,
            scratchIndicatorBounds(nodeBounds, zoom));
    graphics.setColour(kMutedText.withAlpha(0.88f));
    graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize * zoom));
    graphics.drawText(
            "Scratch",
            scratchLabelBounds(nodeBounds, zoom),
            Justification::centredLeft);
}

}
