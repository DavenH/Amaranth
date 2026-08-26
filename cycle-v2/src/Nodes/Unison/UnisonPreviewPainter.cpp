#include "Nodes/Unison/UnisonPreviewPainter.h"

#include "Graph/NodeParameterMap.h"
#include "Nodes/Unison/UnisonNode.h"
#include "UI/Preview/EffectPlotPalette.h"

namespace CycleV2 {

Colour UnisonPreviewPainter::laserColourForPan(float pan) const {
    const Colour leftPan { 0xffff9f43 };
    const Colour centrePan { 0xffc7c7c7 };
    const Colour rightPan { 0xffa56cff };
    const float position = jlimit(0.f, 1.f, pan);
    if (position <= 0.5f) {
        return leftPan.interpolatedWith(centrePan, position * 2.f);
    }
    return centrePan.interpolatedWith(rightPan, position * 2.f - 1.f);
}

std::vector<UnisonPreviewPath> UnisonPreviewPainter::makePaths(
        const Node& node,
        const UnisonPreviewContext& context) const {
    const auto configuration = buildUnisonNodeConfiguration(node.parameters, node.model);
    std::vector<UnisonPreviewPath> paths;
    paths.reserve((size_t) configuration->layout.order);
    for (int index = 0; index < configuration->layout.order; ++index) {
        const auto& voice = configuration->layout[index];
        paths.push_back({
                index,
                voice.detuneCents,
                voice.pan,
                context.pitchEnvelopeUnitValues.empty()
                        ? CycleDsp::UnisonCore::phaseSegments(
                                CycleDsp::UnisonCore::phaseTrajectory(
                                        context.midiNote,
                                        voice.detuneCents,
                                        voice.phaseCycles),
                                context.voiceDurationSeconds)
                        : CycleDsp::UnisonCore::phaseSegmentsForPitchEnvelope(
                                context.midiNote,
                                voice.detuneCents,
                                voice.phaseCycles,
                                context.voiceDurationSeconds,
                                context.pitchEnvelopeUnitValues)
        });
    }
    return paths;
}

void UnisonPreviewPainter::paint(
        Graphics& graphics,
        Rectangle<float> area,
        const Node& node,
        float zoom,
        const UnisonPreviewContext& context) const {
    const bool enabled = NodeParameterMap(node).boolValue("enabled", true);
    const Rectangle<float> background = area.reduced(1.f);
    const Rectangle<float> plot = background.reduced(4.f);
    const auto colour = [enabled](Colour value) {
        return EffectPlotPalette::forEnabledState(value, enabled);
    };

    graphics.setColour(colour(EffectPlotPalette::background));
    graphics.fillRoundedRectangle(background, 4.f);
    graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.22f)));
    graphics.fillRect(plot.getX(), plot.getY(), plot.getWidth(), 1.f);
    graphics.fillRect(plot.getX(), plot.getBottom() - 1.f, plot.getWidth(), 1.f);
    graphics.setColour(colour(EffectPlotPalette::grid.withAlpha(0.58f)));
    graphics.fillRect(plot.getX(), plot.getCentreY() - 0.5f, plot.getWidth(), 1.f);

    const double duration = jmax(0.000001, context.voiceDurationSeconds);
    const auto paths = makePaths(node, context);
    for (const auto& voice : paths) {
        const Colour laser = laserColourForPan(voice.pan);
        Path path;
        for (const auto& segment : voice.segments) {
            const Point<float> start {
                    plot.getX() + (float) (segment.startSeconds / duration) * plot.getWidth(),
                    plot.getCentreY() - (float) segment.startPhaseCycles * plot.getHeight()
            };
            const Point<float> end {
                    plot.getX() + (float) (segment.endSeconds / duration) * plot.getWidth(),
                    plot.getCentreY() - (float) segment.endPhaseCycles * plot.getHeight()
            };
            path.startNewSubPath(start);
            path.lineTo(end);
        }
        graphics.setColour(colour(laser.withAlpha(0.16f)));
        graphics.strokePath(path, PathStrokeType(jmax(2.8f, 4.f * zoom), PathStrokeType::curved));
        graphics.setColour(colour(laser.withAlpha(0.94f)));
        graphics.strokePath(path, PathStrokeType(jmax(1.f, 1.35f * zoom), PathStrokeType::curved));
    }

    if (area.getWidth() >= 260.f && area.getHeight() >= 100.f) {
        const String note = MidiMessage::getMidiNoteName(context.midiNote, true, true, 3);
        graphics.setColour(colour(EffectPlotPalette::label.withAlpha(0.70f)));
        graphics.setFont(FontOptions(10.f));
        graphics.drawText(
                note + "  ·  " + String(context.voiceDurationSeconds, 2) + " s",
                plot.getX(),
                plot.getY() + 4.f,
                plot.getWidth() - 5.f,
                13.f,
                Justification::centredRight);
    }
}

}
