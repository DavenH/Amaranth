#include <cmath>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "VoiceContextCompactEditor.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
const Colour kPanelBackground { 0xff11161c };
const Colour kPanelBorder { 0xff34404d };
constexpr float kLabelWidth = 92.f;
constexpr float kRowHeight = 28.f;
constexpr float kRowGap = 7.f;
constexpr float kExpandedHeaderHeight = 44.f;
constexpr float kSliderReadoutWidth = 92.f;
constexpr float kColumnGap = 12.f;

struct SliderTick {
    float normalized;
    String label;
};

Rectangle<float> nextRow(Rectangle<float>& column) {
    Rectangle<float> row = column.removeFromTop(kRowHeight);
    column.removeFromTop(kRowGap);
    return row;
}

Rectangle<float> sliderTrackBounds(Rectangle<float> row) {
    return row
            .withTrimmedLeft(kLabelWidth + kColumnGap)
            .withTrimmedRight(kSliderReadoutWidth + kColumnGap)
            .reduced(2.f, 0.f);
}

bool portamentoEnabled(const Node& node) {
    return parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";
}

bool isSpectral(const Node& node) {
    return VoiceContextCompactEditor::domain(node).startsWith("spectral");
}

String durationText(double voiceDurationSeconds) {
    const double duration = jmax(0.0, voiceDurationSeconds);
    const int wholeSeconds = roundToInt(duration);
    const bool isWholeSecond = std::abs(duration - (double) wholeSeconds) < 0.0005;
    const String value = isWholeSecond
            ? String(wholeSeconds)
            : String(duration, 2).trimCharactersAtEnd("0").trimCharactersAtEnd(".");
    return value + (std::abs(duration - 1.0) < 0.0005 ? " second" : " seconds");
}

String durationReadout(double voiceDurationSeconds) {
    return String(roundToInt(jmax(0.0, voiceDurationSeconds))) + " s";
}

String pitchText(float semitones) {
    const int rounded = roundToInt(semitones);
    return String(rounded) + (rounded == 1 || rounded == -1
            ? " semitone"
            : " semitones");
}

void drawSourceSelector(Graphics& graphics, Rectangle<float> area, const Node& node) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    Rectangle<float> control = area.reduced(0.f, 2.f);
    const bool spectral = isSpectral(node);
    const Colour waveformColour = colourForDomain(PortDomain::TimeSignal);
    const Colour spectralColour = colourForDomain(PortDomain::SpectralMagnitudeSignal);
    const Colour activeColour = spectral ? spectralColour : waveformColour;

    graphics.setFont(FontOptions(10.8f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText("Domain", labelArea.withTrimmedRight(10.f), Justification::centredRight);

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
        Colour colour,
        const String& readout = {},
        std::initializer_list<SliderTick> ticks = {},
        bool keepEndpointLabelsInside = false) {
    const float trackY = ticks.size() > 0 ? area.getY() + 7.f : area.getCentreY();
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    area.removeFromLeft(kColumnGap);
    Rectangle<float> readoutArea;
    if (readout.isNotEmpty()) {
        readoutArea = area.removeFromRight(kSliderReadoutWidth);
        area.removeFromRight(kColumnGap);
    }
    Rectangle<float> valueArea = area.reduced(2.f, 0.f);
    const float left = valueArea.getX();
    const float right = valueArea.getRight();
    const float knobX = jmap(jlimit(0.f, 1.f, normalized), 0.f, 1.f, left, right);
    const float knobSize = jmax(8.f, area.getHeight() * 0.35f);

    graphics.setFont(FontOptions(11.f, Font::bold));
    graphics.setColour(kMutedText.withAlpha(0.76f));
    graphics.drawText(label, labelArea.withTrimmedRight(10.f), Justification::centredRight);
    graphics.setColour(kMutedText.withAlpha(0.30f));
    graphics.drawLine(Line<float>({ left, trackY }, { right, trackY }), 1.4f);
    graphics.setColour(colour.withAlpha(0.76f));
    graphics.drawLine(Line<float>({ left, trackY }, { knobX, trackY }), 2.2f);
    graphics.fillEllipse(Rectangle<float>(knobSize, knobSize).withCentre({ knobX, trackY }));
    graphics.setFont(FontOptions(9.5f));
    for (const auto& tick : ticks) {
        const float x = jmap(tick.normalized, 0.f, 1.f, left, right);
        graphics.setColour(kMutedText.withAlpha(0.75f));
        graphics.drawVerticalLine(roundToInt(x), trackY + 2.f, trackY + 5.f);
        Rectangle<float> tickBounds(x - 22.f, trackY + 5.f, 44.f, 10.f);
        Justification justification = Justification::centred;
        if (keepEndpointLabelsInside && tick.normalized <= 0.f) {
            tickBounds.setX(x);
            justification = Justification::centredLeft;
        } else if (keepEndpointLabelsInside && tick.normalized >= 1.f) {
            tickBounds.setX(x - tickBounds.getWidth());
            justification = Justification::centredRight;
        }
        graphics.drawText(
                tick.label,
                tickBounds,
                justification);
    }
    if (readout.isNotEmpty()) {
        graphics.setFont(FontOptions(10.5f, Font::bold));
        graphics.setColour(kText.withAlpha(0.88f));
        graphics.drawText(
                readout,
                readoutArea.withTrimmedLeft(10.f),
                Justification::centredLeft);
    }
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
    graphics.drawText(label, labelArea.withTrimmedRight(10.f), Justification::centredRight);
}

void drawStopSlider(
        Graphics& graphics,
        Rectangle<float> area,
        const String& label,
        const std::vector<String>& values,
        const String& value,
        Colour colour) {
    Rectangle<float> labelArea = area.removeFromLeft(kLabelWidth);
    area.removeFromLeft(kColumnGap);
    Rectangle<float> readoutArea = area.removeFromRight(kSliderReadoutWidth);
    area.removeFromRight(kColumnGap);
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
    graphics.drawText(label, labelArea.withTrimmedRight(10.f), Justification::centredRight);
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
    graphics.setFont(FontOptions(10.5f, Font::bold));
    graphics.setColour(kText.withAlpha(0.88f));
    graphics.drawText(
            value,
            readoutArea.withTrimmedLeft(10.f),
            Justification::centredLeft);
}

}

Rectangle<float> VoiceContextCompactEditor::expandedContentBounds(Rectangle<float> panel) {
    panel.removeFromTop(kExpandedHeaderHeight);
    return panel.removeFromTop(218.f).reduced(24.f, 4.f);
}

Rectangle<float> VoiceContextCompactEditor::nodeSelectorBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    const Rectangle<float> body = nodeBounds.withTrimmedTop(42.f * zoom);
    const float width = jmin(nodeBounds.getWidth() - 96.f * zoom, 64.f * zoom);
    return Rectangle<float>(width, 28.f * zoom).withCentre(
            { nodeBounds.getCentreX(), body.getY() + 28.f * zoom });
}

Rectangle<float> VoiceContextCompactEditor::octaveControlBounds(Rectangle<float> panel) {
    Rectangle<float> column = expandedContentBounds(panel);
    nextRow(column);
    return sliderTrackBounds(nextRow(column));
}

Rectangle<float> VoiceContextCompactEditor::voiceLengthControlBounds(Rectangle<float> panel) {
    Rectangle<float> column = expandedContentBounds(panel);
    nextRow(column);
    nextRow(column);
    return sliderTrackBounds(nextRow(column));
}

Rectangle<float> VoiceContextCompactEditor::pitchControlBounds(Rectangle<float> panel) {
    Rectangle<float> column = expandedContentBounds(panel);
    nextRow(column);
    nextRow(column);
    nextRow(column);
    return sliderTrackBounds(nextRow(column));
}

Rectangle<float> VoiceContextCompactEditor::oversamplingControlBounds(Rectangle<float> panel) {
    Rectangle<float> column = expandedContentBounds(panel);
    nextRow(column);
    nextRow(column);
    nextRow(column);
    nextRow(column);
    return sliderTrackBounds(nextRow(column));
}

std::optional<VoiceContextEdit> VoiceContextCompactEditor::sliderEditAt(
        VoiceContextEdit::Control control,
        Rectangle<float> panel,
        float positionX) {
    if (control == VoiceContextEdit::Control::VoiceLength) {
        return VoiceContextEdit { control, String(voiceLengthAt(panel, positionX), 6) };
    }

    Rectangle<float> bounds;
    float minimum;
    float maximum;
    if (control == VoiceContextEdit::Control::Octave) {
        bounds = octaveControlBounds(panel);
        minimum = -2.f;
        maximum = 2.f;
    } else if (control == VoiceContextEdit::Control::Pitch) {
        bounds = pitchControlBounds(panel);
        minimum = -12.f;
        maximum = 12.f;
    } else if (control == VoiceContextEdit::Control::Oversampling) {
        bounds = oversamplingControlBounds(panel);
        const float normalized = jlimit(
                0.f,
                1.f,
                (positionX - bounds.getX()) / jmax(1.f, bounds.getWidth()));
        const String values[] { "1x", "2x", "4x", "8x" };
        return VoiceContextEdit { control, values[jlimit(0, 3, roundToInt(normalized * 3.f))] };
    } else {
        return {};
    }

    const float normalized = jlimit(
            0.f,
            1.f,
            (positionX - bounds.getX()) / jmax(1.f, bounds.getWidth()));
    return VoiceContextEdit { control, String(roundToInt(jmap(normalized, minimum, maximum))) };
}

double VoiceContextCompactEditor::voiceLengthAt(Rectangle<float> panel, float positionX) {
    const Rectangle<float> control = voiceLengthControlBounds(panel);
    const double normalized = jlimit(
            0.0,
            1.0,
            (double) (positionX - control.getX()) / jmax(1.f, control.getWidth()));
    return CycleDsp::voiceLengthSeconds((float) normalized);
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

void VoiceContextCompactEditor::paintExpanded(
        Graphics& graphics,
        Rectangle<float> panel,
        const Node& node,
        double voiceDurationSeconds) {
    graphics.setColour(kPanelBackground);
    graphics.fillRoundedRectangle(panel, 10.f);
    graphics.setColour(kPanelBorder);
    graphics.drawRoundedRectangle(panel.reduced(0.5f), 10.f, 1.f);
    graphics.setColour(kText);
    graphics.setFont(FontOptions(17.f, Font::bold));
    graphics.drawText("VOICE CONTEXT", panel.reduced(18.f, 0.f).removeFromTop(42.f),
            Justification::centredLeft);
    graphics.setColour(kMutedText.withAlpha(0.72f));
    graphics.setFont(FontOptions(18.f));
    graphics.drawText(
            String::fromUTF8("×"),
            Rectangle<float>(22.f, 22.f).withCentre(
                    { panel.getRight() - 18.f, panel.getY() + 15.f }),
            Justification::centred);

    Rectangle<float> column = expandedContentBounds(panel);
    const Colour colour = colourForDomain(PortDomain::PitchSignal);
    const float pitch = parameterValueForNode(node, "pitch", "0").getFloatValue();

    drawSourceSelector(graphics, nextRow(column), node);

    const int octave = jlimit(
            -2,
            2,
            parameterValueForNode(node, "octave", "0").getIntValue());
    drawSlider(
            graphics,
            nextRow(column),
            "Octave",
            (float) (octave + 2) / 4.f,
            colour,
            String(octave),
            {
                    { 0.f, "-2" },
                    { 0.25f, "-1" },
                    { 0.5f, "0" },
                    { 0.75f, "+1" },
                    { 1.f, "+2" }
            });

    drawSlider(
            graphics,
            nextRow(column),
            "Voice Length",
            CycleDsp::voiceLengthUnitValue(voiceDurationSeconds),
            colour,
            durationReadout(voiceDurationSeconds),
            {
                    { 0.f, "0.05 s" },
                    { CycleDsp::voiceLengthUnitValue(1.0), "1 s" },
                    { CycleDsp::voiceLengthUnitValue(7.0), "7 s" },
                    { 1.f, "148 s" }
            },
            true);

    drawSlider(
            graphics,
            nextRow(column),
            "Pitch",
            (pitch + 12.f) / 24.f,
            colour,
            pitchText(pitch));
    drawStopSlider(
            graphics,
            nextRow(column),
            "Oversampling",
            { "1x", "2x", "4x", "8x" },
            parameterValueForNode(node, "oversampling", "1x"),
            colour);
    drawCheckbox(
            graphics,
            nextRow(column),
            "Portamento",
            portamentoEnabled(node));
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
    graphics.setFont(FontOptions(15.1f * zoom, Font::bold));
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

    Rectangle<float> octaveControl = octaveControlBounds(panel);
    nextRow(column);
    if (octaveControl.expanded(8.f, 4.f).contains(position)) {
        return sliderEditAt(VoiceContextEdit::Control::Octave, panel, position.x);
    }

    nextRow(column);
    if (voiceLengthControlBounds(panel).expanded(8.f, 4.f).contains(position)) {
        return VoiceContextEdit {
                VoiceContextEdit::Control::VoiceLength,
                String(voiceLengthAt(panel, position.x), 6)
        };
    }

    Rectangle<float> pitchControl = pitchControlBounds(panel);
    nextRow(column);
    if (pitchControl.expanded(8.f, 4.f).contains(position)) {
        return sliderEditAt(VoiceContextEdit::Control::Pitch, panel, position.x);
    }

    Rectangle<float> oversamplingControl = oversamplingControlBounds(panel);
    nextRow(column);
    if (oversamplingControl.expanded(8.f, 6.f).contains(position)) {
        return sliderEditAt(VoiceContextEdit::Control::Oversampling, panel, position.x);
    }

    Rectangle<float> portamentoControl = nextRow(column).withTrimmedLeft(kLabelWidth);
    if (portamentoControl.expanded(6.f, 4.f).contains(position)) {
        return VoiceContextEdit {
                VoiceContextEdit::Control::Portamento,
                portamentoEnabled(node) ? "0" : "1"
        };
    }

    return {};
}

}
