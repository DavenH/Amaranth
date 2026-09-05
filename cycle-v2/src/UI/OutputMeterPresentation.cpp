#include "UI/OutputMeterPresentation.h"

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
    constexpr float horizontalInsetFraction = 0.14f;
    constexpr float verticalInsetFraction = 0.08f;
    constexpr float channelGapFraction = 0.04f;

    const float horizontalInset = area.getWidth() * horizontalInsetFraction;
    const float verticalInset = area.getHeight() * verticalInsetFraction;
    const juce::Rectangle<float> content = area.reduced(horizontalInset, verticalInset);
    const float preferredGap = juce::jlimit(
            4.f,
            8.f,
            area.getWidth() * channelGapFraction);
    const float channelGap = juce::jmin(preferredGap, content.getWidth());
    const float channelWidth = juce::jmax(0.f, (content.getWidth() - channelGap) * 0.5f);

    return {
            { content.getX(), content.getY(), channelWidth, content.getHeight() },
            { content.getRight() - channelWidth, content.getY(), channelWidth, content.getHeight() }
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

void OutputMeterPresentation::paint(
        juce::Graphics& graphics,
        juce::Rectangle<float> area,
        float leftLevel,
        float rightLevel,
        juce::Colour colour) {
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
}

}
