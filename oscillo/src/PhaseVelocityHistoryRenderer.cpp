#include "PhaseVelocityHistoryRenderer.h"

#include <algorithm>
#include <cmath>

using namespace juce;

#include "GradientColorMap.h"

namespace {
float finiteUnit(float value) {
    return std::isfinite(value) ? jlimit(0.0f, 1.0f, value) : 0.0f;
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

    g.setColour(grid.withAlpha(0.75f));
    g.drawHorizontalLine(area.getCentreY(), (float) area.getX(), (float) area.getRight());
    g.drawHorizontalLine(area.getY() + area.getHeight() / 4, (float) area.getX(), (float) area.getRight());
    g.drawHorizontalLine(area.getY() + area.getHeight() * 3 / 4, (float) area.getX(), (float) area.getRight());

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

        auto getPoint = [&](int index) {
            const float velocity = harmonicIndex < 0
                ? history->weightedValues[(size_t) index]
                : history->values[(size_t) harmonicIndex][(size_t) index] * harmonicScale;
            const float normalized = jlimit(-1.0f, 1.0f, velocity / maxVelocity);
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
    g.setFont(13.0f);
    g.drawText(labelText,
        area.reduced(8).removeFromTop(18), Justification::centredLeft);
}
