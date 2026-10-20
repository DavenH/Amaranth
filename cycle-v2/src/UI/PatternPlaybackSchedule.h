#pragma once

#include "UI/PatternControlScheduler.h"

namespace CycleV2 {

struct PatternPlaybackEvent {
    double timeSeconds {};
    juce::MidiMessage message;
};

struct PatternPlaybackSchedule {
    std::vector<PatternPlaybackEvent> notes;
    std::vector<PatternControlScheduler> controls;

    static PatternPlaybackSchedule prepare(const PresetMidiSequence& sequence);
    static bool precedes(const PatternPlaybackEvent& first, const PatternPlaybackEvent& second);
};

}
