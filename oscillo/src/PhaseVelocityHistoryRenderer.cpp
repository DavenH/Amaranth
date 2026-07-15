#include "PhaseVelocityHistoryRenderer.h"

#include <algorithm>
#include <cmath>

using namespace juce;

#include "GradientColorMap.h"

namespace {
constexpr float kCentsMagnitudeThreshold = 0.08f;

float finiteUnit(float value) {
    return std::isfinite(value) ? jlimit(0.0f, 1.0f, value) : 0.0f;
}

struct CurrentReading {
    float velocity = 0.0f;
    float magnitude = 0.0f;
    bool valid = false;
};

CurrentReading getCurrentReading(
        const std::array<PhaseVelocityHistory, PhaseVelocityHistory::kNumTracks>& histories,
        int harmonicIndex) {
    for (const PhaseVelocityHistory& history : histories) {
        if (!history.active || history.length == 0) {
            continue;
        }

        const size_t sample = (size_t) (history.length - 1);
        if (harmonicIndex < 0) {
            return { history.weightedValues[sample], history.weightedMagnitudes[sample], true };
        }

        const float harmonicScale = 1.0f / (float) (harmonicIndex + 1);
        return {
            history.values[(size_t) harmonicIndex][sample] * harmonicScale,
            history.magnitudes[(size_t) harmonicIndex][sample],
            true
        };
    }
    return {};
}

float velocityToCents(float velocity) {
    const float frequencyRatio = jmax(0.5f, 1.0f + 0.5f * velocity);
    return 1200.0f * std::log2(frequencyRatio);
}
}

void PhaseVelocityHistoryRenderer::draw(
        Graphics& g,
        const Rectangle<int>& area,
        int harmonicIndex,
        const String& labelText,
        const std::array<PhaseVelocityHistory, PhaseVelocityHistory::kNumTracks>& histories,
        int latestSequence,
        const GradientColourMap& colourMap,
        Colour background,
        Colour grid,
        Colour label,
        float maxVelocity) {
    jassert(harmonicIndex == -1
        || isPositiveAndBelow(harmonicIndex, PhaseVelocityHistory::kNumPartials));

    Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(area);
    g.fillAll(background);

    g.setColour(grid.withAlpha(0.22f));
    for (int division = 1; division < 12; ++division) {
        const int x = area.getX() + division * area.getWidth() / 12;
        g.drawVerticalLine(x, (float) area.getY(), (float) area.getBottom());
    }
    for (int division = 1; division < 8; ++division) {
        const int y = area.getY() + division * area.getHeight() / 8;
        g.drawHorizontalLine(y, (float) area.getX(), (float) area.getRight());
    }

    g.setColour(label.withAlpha(0.62f));
    g.drawHorizontalLine(area.getCentreY(), (float) area.getX(), (float) area.getRight());

    std::array<const PhaseVelocityHistory*, PhaseVelocityHistory::kNumTracks> ordered{};
    for (size_t i = 0; i < histories.size(); ++i) {
        ordered[i] = &histories[i];
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const PhaseVelocityHistory* lhs, const PhaseVelocityHistory* rhs) {
            return lhs->sequence < rhs->sequence;
        });

    const float xStep = 6.0f * area.getWidth() / jmax(1, PhaseVelocityHistory::kNumFrames - 1);
    const float halfHeight = 0.5f * area.getHeight();
    const float harmonicScale = harmonicIndex < 0 ? 1.0f : 1.0f / (float) (harmonicIndex + 1);
    const float referenceFrequency = (float) MidiMessage::getMidiNoteInHertz(60);

    for (const PhaseVelocityHistory* history : ordered) {
        if (history->length < 2) {
            continue;
        }

        const float age = (float) (latestSequence - history->sequence);
        const float recency = jlimit(0.0f, 1.0f,
            1.0f - age / (float) (PhaseVelocityHistory::kNumTracks - 1));
        const int colourIndex = roundToInt(recency * (float) (colourMap.size() - 1));
        const Colour trackColour = colourMap.getColour(colourIndex);
        const float trackAlpha = 0.45f + 0.55f * recency;
        const float rawPitchScale = jlimit(0.25f, 4.0f,
            referenceFrequency / (float) MidiMessage::getMidiNoteInHertz(history->midiNote));
        const float pitchScale = rawPitchScale > 1.0f
            ? 1.0f + 0.8f * (rawPitchScale - 1.0f)
            : rawPitchScale;

        auto getPoint = [&](int index) {
            const float velocity = harmonicIndex < 0
                ? history->weightedValues[(size_t) index]
                : history->values[(size_t) harmonicIndex][(size_t) index] * harmonicScale;
            const float normalized = jlimit(-1.0f, 1.0f,
                pitchScale * velocity / maxVelocity);
            return Point<float>(
                (float) area.getX() + xStep * index,
                (float) area.getCentreY() - normalized * halfHeight);
        };

        Path path;
        path.startNewSubPath(getPoint(0));
        for (int i = 1; i < history->length - 1; ++i) {
            const Point<float> current = getPoint(i);
            const Point<float> next = getPoint(i + 1);
            path.quadraticTo(current, Point<float>(
                0.5f * (current.x + next.x), 0.5f * (current.y + next.y)));
        }
        path.lineTo(getPoint(history->length - 1));

        const float endX = getPoint(history->length - 1).x;
        ColourGradient gradient(trackColour, (float) area.getX(), 0.0f,
            trackColour, endX, 0.0f, false);
        gradient.clearColours();
        for (int i = 0; i < history->length; ++i) {
            const float position = (float) i / (float) (history->length - 1);
            const float magnitude = finiteUnit(
                harmonicIndex < 0
                    ? history->weightedMagnitudes[(size_t) i]
                    : history->magnitudes[(size_t) harmonicIndex][(size_t) i]);
            gradient.addColour(position,
                background.interpolatedWith(trackColour, magnitude)
                    .withAlpha(trackAlpha));
        }

        g.setGradientFill(gradient);
        g.strokePath(path, PathStrokeType(history->active ? 3.5f : 1.75f,
            PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour(label);
    g.setFont(FontOptions(15.0f, Font::bold));
    g.drawText(labelText,
        area.reduced(8).removeFromTop(18), Justification::centredLeft);

    const CurrentReading reading = getCurrentReading(histories, harmonicIndex);
    if (reading.valid && reading.magnitude >= kCentsMagnitudeThreshold) {
        const float cents = velocityToCents(reading.velocity);
        const Colour directionColour = cents >= 0.0f
            ? Colour(0xff6fb7ff)
            : Colour(0xffff6b6b);
        const float colourAmount = jlimit(0.0f, 1.0f, std::abs(cents) / 2.0f);
        const Colour centsColour = Colours::white.interpolatedWith(
            directionColour, colourAmount);
        const float fontSize = harmonicIndex < 0 ? 30.0f : 20.0f;
        const int labelWidth = harmonicIndex < 0 ? 140 : 100;

        g.setColour(centsColour);
        g.setFont(FontOptions(Font::getDefaultMonospacedFontName(), fontSize, Font::bold));
        g.drawText(String::formatted("%+.1f", cents),
            area.reduced(8).removeFromRight(labelWidth).removeFromTop(roundToInt(fontSize + 8.0f)),
            Justification::centredRight);
    }
}
