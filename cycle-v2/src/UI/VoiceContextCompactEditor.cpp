#include "UI/Editors/PropertyControls.h"
#include "UI/VoiceContextCompactEditor.h"

#include "UI/CanvasChromeMetrics.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

bool portamentoEnabled(const Node& node) {
    return parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";
}

bool isSpectral(const Node& node) {
    return VoiceContextCompactEditor::domain(node).startsWith("spectral");
}

String durationText(double voiceDurationSeconds) {
    const double duration = jmax(0.0, voiceDurationSeconds);
    const String value = formatPropertyReal(duration);
    return value + (approximatelyEqual(duration, 1.0) ? " second" : " seconds");
}

}

Rectangle<float> VoiceContextCompactEditor::nodeSelectorBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    const Rectangle<float> body = nodeBounds.withTrimmedTop(42.f * zoom);
    const float width = jmin(nodeBounds.getWidth() - 96.f * zoom, 64.f * zoom);
    return Rectangle<float>(width, 28.f * zoom).withCentre(
            { nodeBounds.getCentreX(), body.getY() + 28.f * zoom });
}

String VoiceContextCompactEditor::domain(const Node& node) {
    return parameterValueForNode(node, "domain", "waveform");
}

String VoiceContextCompactEditor::domainLabel(const Node& node) {
    return isSpectral(node) ? "Spectral" : "Waveform";
}

String VoiceContextCompactEditor::nextDomain(const Node& node) {
    return isSpectral(node) ? "waveform" : "spectral";
}

String VoiceContextCompactEditor::summaryLabel(
        const Node& node,
        double voiceDurationSeconds) {
    const String octave = parameterValueForNode(node, "octave", "0");
    String summary = "Octave " + octave + "  ·  " + durationText(voiceDurationSeconds);
    if (portamentoEnabled(node)) {
        summary += "  ·  Glide";
    }
    return summary;
}

void VoiceContextCompactEditor::paintNodeSelector(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom,
        const Node& node) {
    const Rectangle<float> pill = nodeSelectorBounds(nodeBounds, zoom);
    const bool spectral = isSpectral(node);
    const float labelWidth = 82.f * zoom;
    const Rectangle<float> waveformLabel(
            pill.getX() - labelWidth - 9.f * zoom,
            pill.getY(),
            labelWidth,
            pill.getHeight());
    const Rectangle<float> spectralLabel(
            pill.getRight() + 9.f * zoom,
            pill.getY(),
            labelWidth,
            pill.getHeight());
    const Colour waveformColour = colourForDomain(PortDomain::TimeSignal);
    const Colour spectralColour = colourForDomain(PortDomain::SpectralMagnitudeSignal);
    const Colour activeColour = spectral ? spectralColour : waveformColour;
    const float knobSize = pill.getHeight() - 6.f * zoom;
    const Rectangle<float> knob(knobSize, knobSize);
    const Point<float> knobCentre(
            spectral
                    ? pill.getRight() - pill.getHeight() * 0.5f
                    : pill.getX() + pill.getHeight() * 0.5f,
            pill.getCentreY());

    graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize * zoom));
    graphics.setColour(spectral ? kMutedText.withAlpha(0.70f) : kText);
    graphics.drawText("Waveform", waveformLabel, Justification::centredRight);
    graphics.setColour(spectral ? kText : kMutedText.withAlpha(0.70f));
    graphics.drawText("Spectral", spectralLabel, Justification::centredLeft);
    graphics.setColour(Colour(0xff091015).withAlpha(0.96f));
    graphics.fillRoundedRectangle(pill, pill.getHeight() * 0.5f);
    graphics.setColour(activeColour.withAlpha(0.22f));
    graphics.fillRoundedRectangle(pill.reduced(2.f * zoom), pill.getHeight() * 0.5f);
    graphics.setColour(activeColour.withAlpha(0.82f));
    graphics.drawRoundedRectangle(pill, pill.getHeight() * 0.5f, 1.2f * zoom);
    graphics.setColour(activeColour);
    graphics.fillEllipse(knob.withCentre(knobCentre));
    graphics.setColour(Colours::white.withAlpha(0.30f));
    graphics.drawEllipse(knob.withCentre(knobCentre), zoom);
}

void VoiceContextCompactEditor::paintNodeSummary(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom,
        const Node& node,
        double voiceDurationSeconds) {
    Rectangle<float> summary = nodeBounds
            .withTrimmedTop(94.f * zoom)
            .withTrimmedBottom(10.f * zoom)
            .reduced(16.f * zoom, 0.f);
    graphics.setColour(kText.withAlpha(0.88f));
    graphics.setFont(FontOptions(CanvasChromeMetrics::editorTitleFontSize * zoom));
    graphics.drawText(
            summaryLabel(node, voiceDurationSeconds),
            summary,
            Justification::centred);
}

bool VoiceContextCompactEditor::hitNodeSelector(
        Rectangle<float> nodeBounds,
        float zoom,
        Point<float> position) {
    return nodeSelectorBounds(nodeBounds, zoom)
            .expanded(4.f * zoom)
            .contains(position);
}

}
