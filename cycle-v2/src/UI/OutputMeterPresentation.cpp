#include "UI/OutputMeterPresentation.h"

#include <Audio/CycleDsp/EffectParameterMapping.h>

namespace CycleV2 {

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

OutputMeterLayout OutputMeterPresentation::layout(juce::Rectangle<float> area) {
    constexpr float horizontalInsetFraction = 0.08f;
    constexpr float verticalInsetFraction = 0.08f;
    constexpr float faderWidthFraction = 0.22f;
    constexpr float groupGapFraction = 0.04f;
    constexpr float channelGapFraction = 0.035f;

    const float horizontalInset = area.getWidth() * horizontalInsetFraction;
    const float verticalInset = area.getHeight() * verticalInsetFraction;
    juce::Rectangle<float> content = area.reduced(horizontalInset, verticalInset);
    const float faderWidth = juce::jlimit(
            24.f,
            36.f,
            content.getWidth() * faderWidthFraction);
    const float groupGap = juce::jlimit(
            4.f,
            8.f,
            area.getWidth() * groupGapFraction);
    const auto fader = content.removeFromRight(juce::jmin(faderWidth, content.getWidth()));
    content.removeFromRight(juce::jmin(groupGap, content.getWidth()));
    const float channelGap = juce::jmin(
            juce::jlimit(3.f, 6.f, area.getWidth() * channelGapFraction),
            content.getWidth());
    const float channelWidth = juce::jmax(0.f, (content.getWidth() - channelGap) * 0.5f);
    const float labelHeight = juce::jmin(14.f, fader.getHeight() * 0.16f);
    const auto label = fader.withTop(fader.getBottom() - labelHeight);
    const auto faderTravel = fader.withTrimmedBottom(labelHeight).reduced(0.f, 5.f);
    const juce::Rectangle<float> track(
            faderTravel.getCentreX() - 1.f,
            faderTravel.getY(),
            2.f,
            faderTravel.getHeight());

    return {
            { content.getX(), content.getY(), channelWidth, content.getHeight() },
            { content.getRight() - channelWidth, content.getY(), channelWidth, content.getHeight() },
            fader,
            track,
            label
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

float OutputMeterPresentation::gainUnitValueAt(
        juce::Rectangle<float> area,
        float y) {
    const auto track = layout(area).faderTrack;
    if (track.getHeight() <= 0.f) {
        return 0.5f;
    }

    return juce::jlimit(0.f, 1.f, (track.getBottom() - y) / track.getHeight());
}

juce::Rectangle<float> OutputMeterPresentation::gainThumbBounds(
        juce::Rectangle<float> area,
        float gainUnitValue) {
    const auto result = layout(area);
    constexpr float thumbHeight = 7.f;
    const float thumbWidth = juce::jmin(20.f, result.faderHitTarget.getWidth());
    const float y = juce::jmap(
            juce::jlimit(0.f, 1.f, gainUnitValue),
            result.faderTrack.getBottom(),
            result.faderTrack.getY());
    return juce::Rectangle<float>(thumbWidth, thumbHeight)
            .withCentre({ result.faderTrack.getCentreX(), y });
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
        float gainUnitValue) {
    const auto channels = layout(area);
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

    graphics.setColour(juce::Colours::white.withAlpha(0.18f));
    graphics.fillRoundedRectangle(channels.faderTrack, 1.f);

    const float unityY = gainThumbBounds(area, 0.5f).getCentreY();
    graphics.setColour(juce::Colours::white.withAlpha(0.28f));
    graphics.drawHorizontalLine(
            juce::roundToInt(unityY),
            channels.faderTrack.getX() - 4.f,
            channels.faderTrack.getRight() + 4.f);

    const auto thumb = gainThumbBounds(area, gainUnitValue);
    graphics.setColour(colour.withAlpha(0.95f));
    graphics.fillRoundedRectangle(thumb, 2.f);
    graphics.setColour(juce::Colours::black.withAlpha(0.78f));
    graphics.drawHorizontalLine(
            juce::roundToInt(thumb.getCentreY()),
            thumb.getX() + 2.f,
            thumb.getRight() - 2.f);

    graphics.setColour(juce::Colours::white.withAlpha(0.74f));
    graphics.setFont(juce::jmin(10.f, channels.gainLabel.getHeight() * 0.78f));
    graphics.drawFittedText(
            gainLabel(gainUnitValue),
            channels.gainLabel.toNearestInt(),
            juce::Justification::centred,
            1);
}

}
