#pragma once

#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Audio/CycleDsp/UnisonCore.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>

#include <array>
#include <cstddef>

namespace CycleV2 {

struct NoteLifecycleEvent;
struct PreparedOscillatorProcessContext;

struct ChainedCycleRenderRequest {
    int laneIndex {};
    int sampleCount {};
    double angleDelta {};
    double cycleStartSample {};
    CycleDsp::UnisonVoice voice;
    const PreparedOscillatorProcessContext* processContext {};
    size_t blockSampleOffset {};
};

class OscillatorCycleRenderer {
public:
    virtual ~OscillatorCycleRenderer() = default;
    virtual void reset() {}
    virtual void applyLifecycleEvent(const NoteLifecycleEvent&) {}
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
            const PreparedOscillatorProcessContext& context,
            OscillatorCycleRenderer& renderer);
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

    bool renderUntilReady(
            int laneIndex,
            const PreparedOscillatorProcessContext& context,
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
