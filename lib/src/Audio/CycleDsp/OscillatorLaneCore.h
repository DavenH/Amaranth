#pragma once

namespace CycleDsp {

struct ChainedCycleState {
    double cumulativePosition {};
    long sampledFrontier {};
    int samplesThisCycle {};
};

class OscillatorLaneCore {
public:
    static double angleDelta(int midiNote, float detuneCents, double sampleRate);
    static double angleDeltaForPitchUnit(
            int midiNote,
            float detuneCents,
            float pitchUnitValue,
            double sampleRate);
    static void advanceChainedCycle(ChainedCycleState& state, double angleDelta);
};

}
