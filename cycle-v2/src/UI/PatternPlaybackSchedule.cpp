#include <algorithm>
#include <map>
#include <utility>

#include "UI/PatternPlaybackSchedule.h"

namespace CycleV2 {

PatternPlaybackSchedule PatternPlaybackSchedule::prepare(const PresetMidiSequence& sequence) {
    PatternPlaybackSchedule result;
    for (const auto& note : sequence.notes) {
        result.notes.push_back({
                note.startSeconds,
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
        result.controls.push_back(std::move(scheduler));
    }
    std::stable_sort(result.notes.begin(), result.notes.end(), precedes);
    return result;
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
