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
    static double legacyNeutralAngleDelta(int midiNote, double sampleRate);
    static double angleDeltaForPitchUnit(
            int midiNote,
            float detuneCents,
            float pitchUnitValue,
            double sampleRate);
    static int controlFrameStride(
            int controlIntervalSamples,
            double neutralCyclePeriod);
    static float interpolatedFramePortion(
            bool singleFrame,
            long cycleCount,
            int controlStride,
            double futureFramePosition,
            double lanePosition,
            double sharedFrameInterval);
    static bool sharedFrameSaturated(
            bool singleFrame,
            long renderedCycleCount,
            long futureCycleCount,
            double primaryLanePosition,
            double futureFramePosition);
    static bool laneWithinSharedFrame(
            bool singleFrame,
            long renderedCycleCount,
            long futureCycleCount,
            double lanePosition,
            double futureFramePosition);
    static void advanceChainedCycle(ChainedCycleState& state, double angleDelta);
};

}
