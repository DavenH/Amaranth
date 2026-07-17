#include "VoiceContextCompactEditor.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
constexpr float kLabelWidth = 78.f;
constexpr float kRowHeight = 23.f;
constexpr float kRowGap = 3.f;

Rectangle<float> nextRow(Rectangle<float>& column) {
    Rectangle<float> row = column.removeFromTop(kRowHeight);
    column.removeFromTop(kRowGap);
    return row;
}

bool portamentoEnabled(const Node& node) {
    return parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";
}

void drawOctaveSlider(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom) {
    const Colour colour = colourForDomain(PortDomain::PitchSignal);
    const float centreY = area.getCentreY();
    const float left = area.getX() + 2.f * zoom;
    const float right = area.getRight() - 2.f * zoom;
    const float tickHeight = jmax(5.f * zoom, area.getHeight() * 0.20f);
    const float tickStroke = jmax(1.f, 1.15f * zoom);
    const float thumbSize = jmax(11.f * zoom, area.getHeight() * 0.44f);
    const int octave = jlimit(
            -2,
            2,
            parameterValueForNode(node, "octave", "0").getIntValue());
    const float thumbX = jmap((float) (octave + 2), 0.f, 4.f, left, right);

    graphics.setColour(kMutedText.withAlpha(0.28f));
    graphics.drawLine(
            Line<float>({ left, centreY }, { right, centreY }),
            jmax(1.f, 1.4f * zoom));
    graphics.setColour(colour.withAlpha(0.70f));
    graphics.drawLine(
            Line<float>({ left, centreY }, { thumbX, centreY }),
            jmax(1.f, 2.f * zoom));

    for (int i = -2; i <= 2; ++i) {
        const float x = jmap((float) (i + 2), 0.f, 4.f, left, right);
        const bool active = i == octave;

        graphics.setColour(active
                ? colour.withAlpha(0.92f)
                : kMutedText.withAlpha(0.55f));
        graphics.drawLine(
                Line<float>(
                        { x, centreY - tickHeight * 0.5f },
                        { x, centreY + tickHeight * 0.5f }),
                tickStroke);
    }

    graphics.setColour(Colour(0xff071015).withAlpha(0.72f));
    graphics.fillEllipse(Rectangle<float>(
            thumbSize + 4.f * zoom,
            thumbSize + 4.f * zoom).withCentre({ thumbX, centreY }));
    graphics.setColour(colour.withAlpha(0.98f));
    graphics.fillEllipse(Rectangle<float>(thumbSize, thumbSize).withCentre({ thumbX, centreY }));
    graphics.setColour(Colours::white.withAlpha(0.22f));
    graphics.drawEllipse(
            Rectangle<float>(thumbSize, thumbSize).withCentre({ thumbX, centreY }),
            jmax(1.f, zoom));
}

void drawSourceSelector(Graphics& graphics, Rectangle<float> area, const Node& node) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    Rectangle<float> control = area.reduced(0.f, 2.f);
    const bool spectral = VoiceContextCompactEditor::domain(node) == "spectral";
    const Colour waveformColour = colourForDomain(PortDomain::TimeSignal);
    const Colour spectralColour = colourForDomain(PortDomain::SpectralMagnitudeSignal);
    const Colour activeColour = spectral ? spectralColour : waveformColour;

    graphics.setFont(FontOptions(10.8f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText("Source", labelArea, Justification::centredLeft);

    Rectangle<float> waveformLabel = control.removeFromLeft(62.f);
    control.removeFromLeft(8.f);
    Rectangle<float> switchArea = control.removeFromLeft(42.f).reduced(1.f, 2.f);
    control.removeFromLeft(8.f);
    Rectangle<float> spectralLabel = control.removeFromLeft(56.f);
    const float knobSize = switchArea.getHeight() - 4.f;
    const Point<float> knobCentre(
            spectral
                    ? switchArea.getRight() - switchArea.getHeight() * 0.5f
                    : switchArea.getX() + switchArea.getHeight() * 0.5f,
            switchArea.getCentreY());

    graphics.setColour(spectral ? kMutedText.withAlpha(0.62f) : kText.withAlpha(0.92f));
    graphics.drawText("Waveform", waveformLabel, Justification::centredRight);
    graphics.setColour(spectral ? kText.withAlpha(0.92f) : kMutedText.withAlpha(0.62f));
    graphics.drawText("Spectral", spectralLabel, Justification::centredLeft);
    graphics.setColour(Colour(0xff0e1318));
    graphics.fillRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f);
    graphics.setColour(activeColour.withAlpha(0.62f));
    graphics.drawRoundedRectangle(switchArea, switchArea.getHeight() * 0.5f, 1.1f);
    graphics.fillEllipse(Rectangle<float>(knobSize, knobSize).withCentre(knobCentre));
}

void drawSlider(
        Graphics& graphics,
        Rectangle<float> area,
        const String& label,
        float normalized,
        Colour colour) {
    const float trackY = area.getCentreY();
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    Rectangle<float> valueArea = area.reduced(2.f, 0.f);
    const float left = valueArea.getX();
    const float right = valueArea.getRight();
    const float knobX = jmap(jlimit(0.f, 1.f, normalized), 0.f, 1.f, left, right);
    const float knobSize = jmax(8.f, area.getHeight() * 0.35f);

    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(label, labelArea, Justification::centredLeft);
    graphics.setColour(kMutedText.withAlpha(0.30f));
    graphics.drawLine(Line<float>({ left, trackY }, { right, trackY }), 1.4f);
    graphics.setColour(colour.withAlpha(0.76f));
    graphics.drawLine(Line<float>({ left, trackY }, { knobX, trackY }), 2.2f);
    graphics.fillEllipse(Rectangle<float>(knobSize, knobSize).withCentre({ knobX, trackY }));
}

void drawCheckbox(
        Graphics& graphics,
        Rectangle<float> area,
        const String& label,
        bool checked) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    const float box = 15.f;
    const Rectangle<float> placed = Rectangle<float>(box, box).withCentre(
            { area.getX() + box * 0.5f, area.getCentreY() });
    const Colour colour = colourForDomain(PortDomain::PitchSignal);

    graphics.setColour(checked ? colour.withAlpha(0.18f) : Colour(0xff0e1318));
    graphics.fillRoundedRectangle(placed, 3.f);
    graphics.setColour(checked ? colour.withAlpha(0.86f) : kMutedText.withAlpha(0.70f));
    graphics.drawRoundedRectangle(placed, 3.f, 1.3f);

    if (checked) {
        Path tick;
        tick.startNewSubPath(placed.getX() + 4.f, placed.getCentreY());
        tick.lineTo(placed.getCentreX() - 1.f, placed.getBottom() - 4.f);
        tick.lineTo(placed.getRight() - 3.5f, placed.getY() + 4.f);
        graphics.strokePath(
                tick,
                PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(label, labelArea, Justification::centredLeft);
}

void drawStopSlider(
        Graphics& graphics,
        Rectangle<float> area,
        const String& label,
        const std::vector<String>& values,
        const String& value,
        Colour colour) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    Rectangle<float> control = area.reduced(2.f, 0.f);
    const float trackY = control.getCentreY() - 2.f;
    const float left = control.getX();
    const float right = control.getRight();
    int activeIndex {};

    for (int i = 0; i < (int) values.size(); ++i) {
        if (values[(size_t) i] == value) {
            activeIndex = i;
            break;
        }
    }

    const float activeX = values.size() <= 1
            ? left
            : jmap((float) activeIndex, 0.f, (float) values.size() - 1.f, left, right);

    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(label, labelArea, Justification::centredLeft);
    graphics.setColour(kMutedText.withAlpha(0.28f));
    graphics.drawLine(Line<float>({ left, trackY }, { right, trackY }), 1.4f);
    graphics.setColour(colour.withAlpha(0.70f));
    graphics.drawLine(Line<float>({ left, trackY }, { activeX, trackY }), 2.f);

    for (int i = 0; i < (int) values.size(); ++i) {
        const float x = values.size() <= 1
                ? left
                : jmap((float) i, 0.f, (float) values.size() - 1.f, left, right);
        const bool active = i == activeIndex;

        graphics.setColour(active
                ? colour.withAlpha(0.92f)
                : kMutedText.withAlpha(0.52f));
        graphics.fillEllipse(Rectangle<float>(
                active ? 9.f : 5.f,
                active ? 9.f : 5.f).withCentre({ x, trackY }));
        graphics.setFont(FontOptions(8.5f, Font::bold));
        graphics.setColour(active
                ? kText.withAlpha(0.90f)
                : kMutedText.withAlpha(0.66f));
        graphics.drawText(
                values[(size_t) i],
                Rectangle<float>(x - 14.f, trackY + 5.f, 28.f, 12.f),
                Justification::centred);
    }
}

}

Rectangle<float> VoiceContextCompactEditor::expandedContentBounds(Rectangle<float> panel) {
    panel.removeFromTop(30.f);
    return panel.reduced(26.f, 18.f);
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
    return domain(node) == "spectral" ? "Spectral" : "Waveform";
}

String VoiceContextCompactEditor::nextDomain(const Node& node) {
    return domain(node) == "spectral" ? "waveform" : "spectral";
}

void VoiceContextCompactEditor::paintExpanded(
        Graphics& graphics,
        Rectangle<float> panel,
        const Node& node) {
    Rectangle<float> column = expandedContentBounds(panel);
    const Colour colour = colourForDomain(PortDomain::PitchSignal);
    const float pitch = parameterValueForNode(node, "pitch", "0").getFloatValue();

    drawSourceSelector(graphics, nextRow(column), node);

    Rectangle<float> octaveRow = nextRow(column);
    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(
            "Octave",
            octaveRow.removeFromLeft(kLabelWidth),
            Justification::centredLeft);
    drawOctaveSlider(graphics, octaveRow.reduced(2.f, 0.f), node, 1.f);

    drawSlider(
            graphics,
            nextRow(column),
            "Pitch",
            (pitch + 12.f) / 24.f,
            colour);
    drawCheckbox(
            graphics,
            nextRow(column),
            "Portamento",
            portamentoEnabled(node));
    drawStopSlider(
            graphics,
            nextRow(column),
            "Oversampling",
            { "1x", "2x", "4x" },
            parameterValueForNode(node, "oversampling", "1x"),
            colour);
}

void VoiceContextCompactEditor::paintNodeSelector(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom,
        const Node& node) {
    const Rectangle<float> pill = nodeSelectorBounds(nodeBounds, zoom);
    const bool spectral = domain(node) == "spectral";
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

    graphics.setFont(FontOptions(15.1f * zoom, Font::bold));
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

bool VoiceContextCompactEditor::hitNodeSelector(
        Rectangle<float> nodeBounds,
        float zoom,
        Point<float> position) {
    return nodeSelectorBounds(nodeBounds, zoom)
            .expanded(4.f * zoom)
            .contains(position);
}

std::optional<VoiceContextEdit> VoiceContextCompactEditor::editAt(
        const Node& node,
        Rectangle<float> panel,
        Point<float> position) {
    Rectangle<float> column = expandedContentBounds(panel);

    Rectangle<float> sourceControl = nextRow(column)
            .withTrimmedLeft(kLabelWidth)
            .reduced(0.f, 2.f);
    const Rectangle<float> waveformLabel = sourceControl.removeFromLeft(62.f);
    sourceControl.removeFromLeft(8.f);
    const Rectangle<float> switchArea = sourceControl.removeFromLeft(42.f);
    sourceControl.removeFromLeft(8.f);
    const Rectangle<float> spectralLabel = sourceControl.removeFromLeft(56.f);

    if (switchArea.expanded(4.f, 2.f).contains(position)) {
        return VoiceContextEdit { VoiceContextEdit::Control::Domain, nextDomain(node) };
    }

    if (waveformLabel.expanded(4.f, 2.f).contains(position)) {
        return VoiceContextEdit { VoiceContextEdit::Control::Domain, "waveform" };
    }

    if (spectralLabel.expanded(4.f, 2.f).contains(position)) {
        return VoiceContextEdit { VoiceContextEdit::Control::Domain, "spectral" };
    }

    Rectangle<float> octaveControl = nextRow(column)
            .withTrimmedLeft(kLabelWidth)
            .reduced(2.f, 0.f);
    if (octaveControl.expanded(8.f, 4.f).contains(position)) {
        const float left = octaveControl.getX() + 2.f;
        const float right = octaveControl.getRight() - 2.f;
        const float normalized = jlimit(
                0.f,
                1.f,
                (position.x - left) / jmax(1.f, right - left));
        const int octave = jlimit(-2, 2, roundToInt(normalized * 4.f) - 2);
        return VoiceContextEdit { VoiceContextEdit::Control::Octave, String(octave) };
    }

    Rectangle<float> pitchControl = nextRow(column)
            .withTrimmedLeft(kLabelWidth)
            .reduced(2.f, 0.f);
    if (pitchControl.expanded(8.f, 4.f).contains(position)) {
        const float normalized = jlimit(
                0.f,
                1.f,
                (position.x - pitchControl.getX())
                        / jmax(1.f, pitchControl.getWidth()));
        const int pitch = roundToInt(jmap(normalized, -12.f, 12.f));
        return VoiceContextEdit { VoiceContextEdit::Control::Pitch, String(pitch) };
    }

    Rectangle<float> portamentoControl = nextRow(column).withTrimmedLeft(kLabelWidth);
    if (portamentoControl.expanded(6.f, 4.f).contains(position)) {
        return VoiceContextEdit {
                VoiceContextEdit::Control::Portamento,
                portamentoEnabled(node) ? "0" : "1"
        };
    }

    Rectangle<float> oversamplingControl = nextRow(column)
            .withTrimmedLeft(kLabelWidth)
            .reduced(2.f, 0.f);
    if (oversamplingControl.expanded(8.f, 6.f).contains(position)) {
        const float normalized = jlimit(
                0.f,
                1.f,
                (position.x - oversamplingControl.getX())
                        / jmax(1.f, oversamplingControl.getWidth()));
        const int stop = jlimit(0, 2, roundToInt(normalized * 2.f));
        const String value = stop == 0 ? "1x" : (stop == 1 ? "2x" : "4x");
        return VoiceContextEdit { VoiceContextEdit::Control::Oversampling, value };
    }

    return {};
}

}
