#include <algorithm>
#include <map>
#include <utility>

#include "UI/PatternPlaybackSchedule.h"

namespace CycleV2 {

namespace {

class PatternScheduleSink final : public MidiEventSink {
public:
    bool enqueueMidiMessage(const juce::MidiMessage& message, MidiEventSource source) override {
        return enqueueMidiMessageAt(message, source, 0.0);
    }

    bool enqueueMidiMessageAt(
            const juce::MidiMessage& message,
            MidiEventSource,
            double timestampSeconds) override {
        if (events.size() >= RealtimeMidiEventQueue::capacity) {
            return false;
        }
        events.push_back({ timestampSeconds, message });
        return true;
    }

    void releaseMidiSource(MidiEventSource) override {}

    std::vector<PatternPlaybackEvent> events;
};

}

PatternPlaybackSchedule PatternPlaybackSchedule::prepare(
        const PresetMidiSequence& sequence, double startSeconds) {
    PatternPlaybackSchedule result;
    for (const auto& note : sequence.notes) {
        if (note.startSeconds + note.durationSeconds <= startSeconds) {
            continue;
        }
        result.notes.push_back({
                juce::jmax(startSeconds, note.startSeconds),
                juce::MidiMessage::noteOn(1, note.pitch, (juce::uint8) note.velocity) });
        result.notes.push_back({
                note.startSeconds + note.durationSeconds,
                juce::MidiMessage::noteOff(1, note.pitch) });
    }
    std::map<int, std::vector<PresetMidiControl>> controlLanes;
    for (const auto& control : sequence.controls) {
        controlLanes[control.controller].push_back(control);
    }
    result.controls.reserve(controlLanes.size());
    for (auto& lane : controlLanes) {
        PatternControlScheduler scheduler;
        scheduler.reset(std::move(lane.second), lane.first, sequence.durationSeconds);
        scheduler.seekTo(startSeconds);
        result.controls.push_back(std::move(scheduler));
    }
    std::stable_sort(result.notes.begin(), result.notes.end(), precedes);
    return result;
}

std::optional<std::vector<PatternPlaybackEvent>> PatternPlaybackSchedule::expand(
        const PresetMidiSequence& sequence, juce::String& error) {
    auto prepared = prepare(sequence);
    PatternScheduleSink sink;
    for (auto& scheduler : prepared.controls) {
        if (!scheduler.scheduleUntil(sequence.durationSeconds, 0.0, sink)) {
            error = "Expanded pattern exceeds MIDI queue capacity";
            return std::nullopt;
        }
    }
    for (const auto& event : prepared.notes) {
        if (!sink.enqueueMidiMessageAt(event.message, MidiEventSource::PatternPlayback, event.timeSeconds)) {
            error = "Expanded pattern exceeds MIDI queue capacity";
            return std::nullopt;
        }
    }
    std::stable_sort(sink.events.begin(), sink.events.end(), precedes);
    error.clear();
    return std::move(sink.events);
}

bool PatternPlaybackSchedule::precedes(
        const PatternPlaybackEvent& first,
        const PatternPlaybackEvent& second) {
    if (first.timeSeconds != second.timeSeconds) {
        return first.timeSeconds < second.timeSeconds;
    }
    const auto priority = [](const juce::MidiMessage& message) {
        return message.isNoteOff() ? 0 : (message.isNoteOn() ? 2 : 1);
    };
    return priority(first.message) < priority(second.message);
}

}
