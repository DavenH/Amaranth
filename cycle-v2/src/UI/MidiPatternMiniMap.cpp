#include <algorithm>

#include "UI/MidiPatternMiniMap.h"
#include "UI/CanvasChromePalette.h"

namespace CycleV2::MidiPatternMiniMap {

juce::Range<int> pitchRange(
        const PresetMidiSequence& sequence, int minimumSpan, int margin) {
    if (sequence.notes.empty()) {
        return { 0, 128 };
    }
    int lowest = sequence.notes.front().pitch;
    int highest = lowest;
    for (const auto& note : sequence.notes) {
        lowest = juce::jmin(lowest, note.pitch);
        highest = juce::jmax(highest, note.pitch);
    }
    lowest = juce::jmax(0, lowest - margin);
    highest = juce::jmin(127, highest + margin);
    if (highest - lowest + 1 < minimumSpan) {
        const int centre = (lowest + highest) / 2;
        lowest = juce::jlimit(0, 128 - minimumSpan, centre - minimumSpan / 2);
        highest = lowest + minimumSpan - 1;
    }
    return { lowest, highest + 1 };
}

void paintNotes(
        juce::Graphics& graphics,
        const PresetMidiSequence& sequence,
        juce::Rectangle<float> bounds,
        juce::Range<int> pitches) {
    juce::Graphics::ScopedSaveState save(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());
    for (const auto& note : sequence.notes) {
        const float x = bounds.getX() + bounds.getWidth()
                * (float) (note.startSeconds / sequence.durationSeconds);
        const float width = juce::jmax(2.f, bounds.getWidth()
                * (float) (note.durationSeconds / sequence.durationSeconds));
        const float y = bounds.getBottom() - bounds.getHeight()
                * ((float) (note.pitch - pitches.getStart()) + 0.5f)
                / (float) pitches.getLength();
        graphics.setColour(CanvasChromePalette::focus.withAlpha(
                0.55f + 0.42f * note.velocity / 127.f));
        graphics.fillRoundedRectangle({ x, y - 1.5f, width, 3.f }, 1.5f);
    }
}

void paintControls(
        juce::Graphics& graphics,
        const PresetMidiSequence& sequence,
        juce::Rectangle<float> bounds) {
    if (sequence.controls.empty()) {
        return;
    }
    const bool hasModWheel = std::any_of(sequence.controls.begin(),
            sequence.controls.end(), [](const PresetMidiControl& control) {
                return control.controller == 1;
            });
    const int lane = hasModWheel ? 1 : sequence.controls.front().controller;
    std::vector<const PresetMidiControl*> points;
    for (const auto& control : sequence.controls) {
        if (control.controller == lane) {
            points.push_back(&control);
        }
    }
    std::stable_sort(points.begin(), points.end(),
            [](const auto* first, const auto* second) {
                return first->timeSeconds < second->timeSeconds;
            });
    juce::Path path;
    for (size_t index = 0; index < points.size(); ++index) {
        const auto& point = *points[index];
        const float x = bounds.getX() + bounds.getWidth()
                * (float) (point.timeSeconds / sequence.durationSeconds);
        const float y = bounds.getBottom() - bounds.getHeight()
                * point.value / 127.f;
        if (index == 0) {
            path.startNewSubPath(x, y);
        } else {
            path.lineTo(x, y);
        }
    }
    juce::Graphics::ScopedSaveState save(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());
    graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.84f));
    graphics.strokePath(path, juce::PathStrokeType(1.1f));
    if (points.size() == 1) {
        const auto& point = *points.front();
        const float x = bounds.getX() + bounds.getWidth()
                * (float) (point.timeSeconds / sequence.durationSeconds);
        const float y = bounds.getBottom() - bounds.getHeight()
                * point.value / 127.f;
        graphics.fillEllipse(x - 1.5f, y - 1.5f, 3.f, 3.f);
    }
}

}
