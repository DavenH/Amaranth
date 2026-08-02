#include "OscillatorLaneCore.h"

#include "UnisonCore.h"

namespace CycleDsp {

double OscillatorLaneCore::angleDelta(
        int midiNote,
        float detuneCents,
        double sampleRate) {
    if (sampleRate <= 0.0) {
        return 0.0;
    }
    return UnisonCore::frequencyForMidiNote(midiNote, detuneCents) / sampleRate;
}

void OscillatorLaneCore::advanceChainedCycle(
        ChainedCycleState& state,
        double angleDelta) {
    if (angleDelta <= 0.0) {
        state.samplesThisCycle = 0;
        return;
    }

    const double nextPosition = state.cumulativePosition + 1.0 / angleDelta;
    state.samplesThisCycle = (int) nextPosition - (int) state.cumulativePosition;
    state.cumulativePosition = nextPosition;
    state.sampledFrontier = (long) nextPosition;
}

}
