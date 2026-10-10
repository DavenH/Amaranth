#include "UI/PatternControlScheduler.h"

#include <algorithm>
#include <utility>

namespace CycleV2 {

void PatternControlScheduler::reset(
        std::vector<PresetMidiControl> nextPoints,
        int nextController,
        double nextDurationSeconds) {
    points = std::move(nextPoints);
    std::stable_sort(points.begin(), points.end(),
            [](const PresetMidiControl& first, const PresetMidiControl& second) {
                return first.timeSeconds < second.timeSeconds;
            });
    controller = nextController;
    durationSeconds = nextDurationSeconds;
    nextStep = 0;
    segment = 0;
    lastValue = -1;
}

bool PatternControlScheduler::scheduleUntil(
        double elapsedSeconds,
        double startSeconds,
        MidiEventSink& sink) {
    if (points.empty()) {
        return true;
    }
    const double horizon = juce::jmin(elapsedSeconds, durationSeconds);
    while ((double) nextStep * stepSeconds <= horizon) {
        const double time = (double) nextStep * stepSeconds;
        while (segment + 1 < points.size()
                && points[segment + 1].timeSeconds <= time) {
            ++segment;
        }

        const auto& first = points[segment];
        int value = first.value;
        if (segment + 1 < points.size()) {
            const auto& second = points[segment + 1];
            const double span = second.timeSeconds - first.timeSeconds;
            if (span > 0.0 && time > first.timeSeconds) {
                const double fraction = juce::jlimit(0.0, 1.0,
                        (time - first.timeSeconds) / span);
                value = juce::roundToInt(first.value
                        + (second.value - first.value) * fraction);
            }
        }
        if (value != lastValue) {
            if (!sink.enqueueMidiMessageAt(
                    juce::MidiMessage::controllerEvent(1, controller, value),
                    MidiEventSource::PatternPlayback,
                    startSeconds + time)) {
                return false;
            }
            lastValue = value;
        }
        ++nextStep;
    }
    return true;
}

}
