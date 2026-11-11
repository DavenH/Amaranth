#include "UI/OutputMeterPresentation.h"
#include "UI/CanvasChromePalette.h"

#include <Audio/CycleDsp/EffectParameterMapping.h>

namespace CycleV2 {

using juce::jmin;
using juce::jmax;

namespace {

juce::Colour segmentColour(float normalized, juce::Colour baseColour) {
    if (normalized > 0.78f) {
        return juce::Colour(0xffff705f);
    }
    if (normalized > 0.58f) {
        return juce::Colour(0xfff4d35e);
    }
    return baseColour;
}

void paintSegment(
        juce::Graphics& graphics,
        juce::Rectangle<float> segment,
        juce::Rectangle<float> fill,
        juce::Colour colour) {
    graphics.setColour(colour.withAlpha(0.14f));
    graphics.fillRoundedRectangle(segment, 1.4f);

    const auto lit = segment.getIntersection(fill);
    if (!lit.isEmpty()) {
        graphics.setColour(colour.withAlpha(0.82f));
        graphics.fillRoundedRectangle(
                lit,
                juce::jmin(1.4f, lit.getHeight() * 0.5f));
    }
}

void paintChannel(
        juce::Graphics& graphics,
        juce::Rectangle<float> bounds,
        float level,
        juce::Colour colour) {
    constexpr int segments = 12;
    const auto fill = OutputMeterPresentation::fillBounds(bounds, level);
    const float gap = juce::jmax(1.f, bounds.getHeight() * 0.015f);
    const float segmentHeight = (bounds.getHeight() - gap * (float) (segments - 1))
            / (float) segments;

    for (int index = 0; index < segments; ++index) {
        const int levelIndex = segments - 1 - index;
        const juce::Rectangle<float> segment(
                bounds.getX(),
                bounds.getY() + (float) index * (segmentHeight + gap),
                bounds.getWidth(),
                segmentHeight);
        const float normalized = (float) levelIndex / (float) (segments - 1);
        const juce::Colour levelColour = segmentColour(normalized, colour);
        paintSegment(graphics, segment, fill, levelColour);
    }
}

}

bool OutputMeterBallistics::update(OutputMeterLevels measured) {
    const OutputMeterLevels next {
            nextLevel(currentLevels.left, measured.left),
            nextLevel(currentLevels.right, measured.right)
    };
    const bool changed = next != currentLevels;
    currentLevels = next;
    return changed;
}

void OutputMeterBallistics::reset() {
    currentLevels = {};
}

float OutputMeterBallistics::nextLevel(float current, float measured) {
    constexpr float releaseMultiplier = 0.8f;
    constexpr float silenceFloor = 0.001f;
    const float target = juce::jlimit(0.f, 1.f, measured);
    if (target >= current) {
        return target;
    }

    const float released = current * releaseMultiplier;
    return released > silenceFloor ? juce::jmax(target, released) : target;
}

OutputMeterLayout OutputMeterPresentation::layout(
        juce::Rectangle<float> area,
        float zoom) {
    const auto content = area.reduced(6.f * zoom);
    const float labelHeight = jmin(20.f * zoom, content.getHeight() * 0.25f);
    const auto controls = content.withTrimmedBottom(labelHeight);
    const float diameter = jmax(0.f, jmin(58.f * zoom,
            jmin(controls.getHeight(), controls.getWidth() - 32.f * zoom)));
    const auto dial = juce::Rectangle<float>(diameter, diameter).withCentre(controls.getCentre());
    const float meterWidth = jmin(9.f * zoom, content.getWidth() * 0.1f);
    const float gap = jmin(8.f * zoom, content.getWidth() * 0.05f);
    return {
            { dial.getX() - gap - meterWidth, controls.getY(), meterWidth, controls.getHeight() },
            { dial.getRight() + gap, controls.getY(), meterWidth, controls.getHeight() },
            dial,
            dial.reduced(3.f * zoom),
            content.withTop(content.getBottom() - labelHeight)
    };
}

float OutputMeterPresentation::displayLevelForAmplitude(float amplitude) {
    constexpr float floorDecibels = -60.f;
    const float clamped = juce::jlimit(0.f, 1.f, amplitude);
    if (clamped <= 0.f) {
        return 0.f;
    }

    const float decibels = juce::Decibels::gainToDecibels(clamped, floorDecibels);
    return juce::jlimit(0.f, 1.f, (decibels - floorDecibels) / -floorDecibels);
}

juce::Rectangle<float> OutputMeterPresentation::fillBounds(
        juce::Rectangle<float> channelBounds,
        float level) {
    const float height = channelBounds.getHeight() * juce::jlimit(0.f, 1.f, level);
    return channelBounds.withTop(channelBounds.getBottom() - height);
}

juce::Rectangle<float> OutputMeterPresentation::gainIndicatorBounds(
        juce::Rectangle<float> area,
        float gainUnitValue,
        float zoom) {
    const auto dial = layout(area, zoom).gainDial;
    const float angle = juce::MathConstants<float>::pi * (1.5f * gainUnitValue - 0.75f);
    const float radius = dial.getWidth() * 0.38f;
    const auto point = dial.getCentre() + juce::Point<float>(std::sin(angle), -std::cos(angle)) * radius;
    return juce::Rectangle<float>(4.f * zoom, 4.f * zoom).withCentre(point);
}

juce::String OutputMeterPresentation::gainLabel(float gainUnitValue) {
    const float decibels = CycleDsp::outputGainDecibels(gainUnitValue);
    const int rounded = juce::roundToInt(decibels);
    if (rounded == 0) {
        return "0 dB";
    }

    return juce::String(rounded > 0 ? "+" : "") + juce::String(rounded) + " dB";
}

void OutputMeterPresentation::paint(
        juce::Graphics& graphics,
        juce::Rectangle<float> area,
        float leftLevel,
        float rightLevel,
        juce::Colour colour,
        float gainUnitValue,
        float zoom) {
    const auto channels = layout(area, zoom);
    paintChannel(
            graphics,
            channels.left,
            displayLevelForAmplitude(leftLevel),
            colour);
    paintChannel(
            graphics,
            channels.right,
            displayLevelForAmplitude(rightLevel),
            colour);

    const auto dial = channels.gainDial;
    graphics.setColour(CanvasChromePalette::raisedSurface);
    graphics.fillEllipse(dial);
    graphics.setColour(CanvasChromePalette::strongBorder);
    graphics.drawEllipse(dial, jmax(1.f, zoom));
    juce::Path arc;
    const float start = -juce::MathConstants<float>::pi * 0.75f;
    const float finish = start + juce::MathConstants<float>::pi * 1.5f * gainUnitValue;
    arc.addCentredArc(dial.getCentreX(), dial.getCentreY(), dial.getWidth() * 0.5f,
            dial.getHeight() * 0.5f, 0.f, start, finish, true);
    graphics.setColour(colour);
    graphics.strokePath(arc, juce::PathStrokeType(jmax(1.5f, 2.f * zoom)));
    const auto indicator = gainIndicatorBounds(area, gainUnitValue, zoom);
    graphics.drawLine(juce::Line<float>(dial.getCentre(), indicator.getCentre()), jmax(1.f, zoom));
    graphics.fillEllipse(indicator);

    graphics.setColour(juce::Colours::white.withAlpha(0.74f));
    graphics.setFont(juce::jmax(8.5f, juce::jmin(
            12.f * zoom, channels.gainLabel.getHeight() * 0.85f)));
    graphics.drawFittedText(
            gainLabel(gainUnitValue),
            channels.gainLabel.toNearestInt(),
            juce::Justification::centred,
            1);
}

}
