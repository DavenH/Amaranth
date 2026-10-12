#include <cstdlib>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include "UI/Editors/PropertyControls.h"
#include "UI/VoiceContextCompactEditor.h"
#include "UI/EnvelopePurposeIconRenderer.h"
#include "UI/NodePortGeometry.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };
const Colour kBlue { 0xff5f9ee6 };
const Colour kRed { 0xffda6875 };
const Colour kYellow { 0xffe7c36a };

bool portamentoEnabled(const Node& node) {
    return parameterValueForNode(node, "portamento", "0") == "1"
            || parameterValueForNode(node, "portamento", "false") == "true";
}

String durationText(double voiceDurationSeconds) {
    const double duration = jmax(0.0, voiceDurationSeconds);
    const String value = formatPropertyReal(duration);
    return value + (approximatelyEqual(duration, 1.0) ? " second" : " seconds");
}

double voiceDurationSeconds(const Node& node) {
    return CycleDsp::voiceLengthSeconds(
            parameterValueForNode(
                    node,
                    "voiceLength",
                    String(CycleDsp::voiceLengthUnitValue(1.0))).getFloatValue());
}

void paintDuration(
        Graphics& graphics,
        Rectangle<float> summary,
        float zoom,
        const Node& node) {
    const float iconSize = 21.f * zoom;
    const Point<float> centre {
            summary.getX() + iconSize * 0.5f,
            summary.getY() + 20.f * zoom
    };
    graphics.setColour(kMutedText);
    graphics.drawEllipse(
            Rectangle<float>(iconSize, iconSize).withCentre(centre),
            jmax(1.f, 1.8f * zoom));
    graphics.drawLine(
            centre.x, centre.y, centre.x, centre.y - 6.f * zoom,
            jmax(1.f, 1.6f * zoom));
    graphics.drawLine(
            centre.x, centre.y,
            centre.x + 5.f * zoom, centre.y + 2.f * zoom,
            jmax(1.f, 1.6f * zoom));

    graphics.setColour(kText);
    graphics.setFont(FontOptions(22.f * zoom));
    graphics.drawText(
            formatPropertyReal(jmax(0.0, voiceDurationSeconds(node))) + "s",
            Rectangle<float>(summary.getX() + 30.f * zoom,
                    summary.getY() + 1.f * zoom,
                    summary.getWidth() - 30.f * zoom,
                    39.f * zoom),
            Justification::centredLeft);
}

void paintOctave(
        Graphics& graphics,
        Point<float> origin,
        float zoom,
        int octave) {
    const float direction = octave > 0 ? -1.f : 1.f;
    graphics.setColour(kBlue);
    graphics.drawLine(
            origin.x, origin.y - 5.f * zoom * direction,
            origin.x, origin.y + 5.f * zoom * direction,
            jmax(1.f, 2.f * zoom));
    for (const float side : { -1.f, 1.f }) {
        graphics.drawLine(
                origin.x + side * 4.f * zoom, origin.y,
                origin.x, origin.y - 5.f * zoom * direction,
                jmax(1.f, 2.f * zoom));
    }
    graphics.setColour(kText);
    graphics.setFont(FontOptions(17.f * zoom));
    graphics.drawText(
            String(std::abs(octave)),
            Rectangle<float>(origin.x + 10.f * zoom, origin.y - 12.f * zoom,
                    20.f * zoom, 24.f * zoom),
            Justification::centredLeft);
}

void paintGlide(Graphics& graphics, Point<float> origin, float zoom) {
    Path glide;
    glide.startNewSubPath(origin.x, origin.y + 5.f * zoom);
    glide.cubicTo(
            origin.x + 14.f * zoom, origin.y + 5.f * zoom,
            origin.x + 14.f * zoom, origin.y - 5.f * zoom,
            origin.x + 31.f * zoom, origin.y - 5.f * zoom);
    graphics.setColour(kYellow);
    graphics.strokePath(glide, PathStrokeType(jmax(1.f, 2.f * zoom)));
}

}

Rectangle<float> VoiceContextCompactEditor::summaryBounds(
        Rectangle<float> nodeBounds,
        float zoom) {
    return nodeBounds
            .withTrimmedTop(66.f * zoom)
            .withTrimmedBottom(35.f * zoom)
            .withTrimmedLeft(65.f * zoom)
            .withTrimmedRight(15.f * zoom);
}

Rectangle<float> VoiceContextCompactEditor::portIconBounds(
        Rectangle<float> nodeBounds,
        float zoom,
        int portIndex) {
    const float size = 19.f * zoom;
    const float centreY = nodeBounds.getY()
            + (NodePortGeometry::firstSidePortOffset
                    + (float) portIndex * NodePortGeometry::sidePortSpacing) * zoom;
    return Rectangle<float>(size, size).withCentre({
            nodeBounds.getX() + 29.f * zoom,
            centreY
    });
}

String VoiceContextCompactEditor::summaryLabel(const Node& node) {
    const String octave = parameterValueForNode(node, "octave", "0");
    String summary = "Octave " + octave + "  ·  " + durationText(voiceDurationSeconds(node));
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
    paintDuration(graphics, summary, zoom, node);

    const int octave = parameterValueForNode(node, "octave", "0").getIntValue();
    const float detailY = summary.getY() + 55.f * zoom;
    if (octave != 0) {
        paintOctave(graphics, { summary.getX() + 4.f * zoom, detailY }, zoom, octave);
    }

    if (portamentoEnabled(node)) {
        const float glideX = summary.getX() + (octave != 0 ? 67.f : 2.f) * zoom;
        paintGlide(graphics, { glideX, detailY }, zoom);
    }
}

void VoiceContextCompactEditor::paintPortIcons(
        Graphics& graphics,
        Rectangle<float> nodeBounds,
        float zoom) {
    const Rectangle<float> modulation = portIconBounds(nodeBounds, zoom, 0);
    const float dotSize = 5.f * zoom;
    for (int index = 0; index < 3; ++index) {
        graphics.setColour(index == 0 ? kYellow : index == 1 ? kRed : kBlue);
        graphics.fillEllipse(Rectangle<float>(dotSize, dotSize).withCentre({
                modulation.getX() + (4.f + (float) index * 5.5f) * zoom,
                modulation.getCentreY() + (index == 1 ? -4.f : 3.f) * zoom
        }));
    }

    const Rectangle<float> unison = portIconBounds(nodeBounds, zoom, 1);
    graphics.setColour(kBlue.withAlpha(0.9f));
    for (int index = 0; index < 3; ++index) {
        const float spread = (float) index - 1.f;
        graphics.drawLine(
                unison.getX() + 2.f * zoom,
                unison.getCentreY(),
                unison.getRight() - 2.f * zoom,
                unison.getCentreY() + spread * 7.f * zoom,
                jmax(1.f, 1.5f * zoom));
    }

    EnvelopePurposeIconRenderer::paint(
            graphics,
            EnvelopePurpose::Pitch,
            portIconBounds(nodeBounds, zoom, 2));
    EnvelopePurposeIconRenderer::paint(
            graphics,
            EnvelopePurpose::Scratch,
            portIconBounds(nodeBounds, zoom, 3));
}

}
