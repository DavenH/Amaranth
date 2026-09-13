#include "OscillatorLaneCore.h"

#include "UnisonCore.h"

#include <algorithm>

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

double OscillatorLaneCore::legacyNeutralAngleDelta(
        int midiNote,
        double sampleRate) {
    if (sampleRate <= 0.0) {
        return 0.0;
    }
    const float frequency = (float) UnisonCore::frequencyForMidiNote(midiNote);
    return frequency / sampleRate;
}

double OscillatorLaneCore::angleDeltaForPitchUnit(
        int midiNote,
        float detuneCents,
        float pitchUnitValue,
        double sampleRate) {
    if (sampleRate <= 0.0) {
        return 0.0;
    }
    const double pitchSemitones = UnisonCore::pitchSemitonesForUnitValue(
            std::clamp((double) pitchUnitValue, 0.01, 0.99));
    return UnisonCore::frequencyForMidiPitch(
            midiNote + pitchSemitones,
            detuneCents) / sampleRate;
}

int OscillatorLaneCore::controlFrameStride(
        int controlIntervalSamples,
        double neutralCyclePeriod) {
    if (controlIntervalSamples <= 0 || neutralCyclePeriod <= 0.0) {
        return 1;
    }
    return std::max(
            1,
            (int) (controlIntervalSamples / neutralCyclePeriod + 0.5));
}

float OscillatorLaneCore::interpolatedFramePortion(
        bool singleFrame,
        long cycleCount,
        int controlStride,
        double futureFramePosition,
        double lanePosition,
        double sharedFrameInterval) {
    if (controlStride <= 0 || sharedFrameInterval <= 0.0) {
        return 0.f;
    }

    const double portion = singleFrame
            ? (cycleCount % controlStride) / (float) controlStride
            : 1.0 - (futureFramePosition - lanePosition)
                    / (float) sharedFrameInterval;
    return std::clamp((float) portion, 0.f, 1.f);
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
