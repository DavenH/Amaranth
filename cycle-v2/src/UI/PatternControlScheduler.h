#pragma once

#include <cstddef>
#include <vector>

#include "Graph/PresetPresentation.h"
#include "Runtime/RealtimeMidiEventQueue.h"

namespace CycleV2 {

class PatternControlScheduler {
public:
    void reset(std::vector<PresetMidiControl> points, int controller,
            double durationSeconds);
    void seekTo(double elapsedSeconds);
    bool scheduleUntil(double elapsedSeconds, double startSeconds,
            MidiEventSink& sink);

private:
    static constexpr double stepSeconds = 0.005;

    std::vector<PresetMidiControl> points;
    int controller { 1 };
    double durationSeconds {};
    size_t nextStep {};
    double nextTimeSeconds {};
    size_t segment {};
    int lastValue { -1 };
};

}
