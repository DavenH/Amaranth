#pragma once

#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Audio/CycleDsp/UnisonCore.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>

#include <array>
#include <cstddef>

namespace CycleV2 {

struct ChainedCycleRenderRequest {
    int laneIndex {};
    int sampleCount {};
    double angleDelta {};
    double cycleStartSample {};
    CycleDsp::UnisonVoice voice;
};

class OscillatorCycleRenderer {
public:
    virtual ~OscillatorCycleRenderer() = default;
    virtual void renderCycle(
            const ChainedCycleRenderRequest& request,
            Buffer<float> left,
            Buffer<float> right) = 0;
};

class ChainedOscillatorRegionRuntime {
public:
    bool prepare(
            size_t maximumFrameCountToUse,
            int maximumCycleSamples,
            double sampleRate,
            const CycleDsp::UnisonVoiceLayout& layout);
    void reset();
    bool process(
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right,
            OscillatorCycleRenderer& renderer);

private:
    struct LaneState {
        CycleDsp::ChainedCycleState clock;
        std::array<ReadWriteBuffer, 2> buffers;
    };

    double angleDeltaFor(
            int midiNote,
            const CycleDsp::UnisonVoice& voice,
            float pitchUnitValue) const;
    bool renderUntilReady(
            int laneIndex,
            int midiNote,
            Buffer<float> pitchEnvelope,
            size_t frameCount,
            OscillatorCycleRenderer& renderer);

    size_t maximumFrameCount {};
    int maximumCycleSamples {};
    double sampleRate { 44100.0 };
    CycleDsp::UnisonVoiceLayout layout;
    std::array<LaneState, CycleDsp::maximumUnisonOrder> lanes;
    ScopedAlloc<float> laneBufferMemory;
    ScopedAlloc<float> scratchMemory;
};

}
