#pragma once

#include "Runtime/SpectralOscillatorFrameRenderer.h"

#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Audio/CycleDsp/UnisonCore.h>
#include <Array/RingBuffer.h>
#include <Array/ScopedAlloc.h>

#include <array>
#include <cstddef>

namespace CycleV2 {

class SpectralOscillatorRegionRuntime {
public:
    bool prepare(
            size_t maximumFrameCountToUse,
            int maximumCycleSamplesToUse,
            int maximumFixedFrameSizeToUse,
            double sampleRateToUse,
            const CycleDsp::UnisonVoiceLayout& layoutToUse);
    void reset();
    bool process(
            const PreparedOscillatorProcessContext& context,
            SpectralOscillatorFrameRenderer& renderer);
    bool process(
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right,
            SpectralOscillatorFrameRenderer& renderer);

private:
    static constexpr int legacyControlIntervalSamples = 16;

    struct LaneState {
        CycleDsp::ChainedCycleState clock;
        std::array<ReadWriteBuffer, 2> buffers;
        std::array<std::array<float, 7>, 2> padding {};
        std::array<double, 2> samplingSpillover {};
        std::array<Buffer<float>, 2> lastLerpHalf;
    };

    int fixedFrameSizeFor(int midiNote) const;
    bool initializeSharedFrames(
            const PreparedOscillatorProcessContext& context,
            SpectralOscillatorFrameRenderer& renderer);
    bool refreshSharedFramesThrough(
            double cycleStart,
            const PreparedOscillatorProcessContext& context,
            SpectralOscillatorFrameRenderer& renderer);
    bool renderCyclesUntilReady(
            const PreparedOscillatorProcessContext& context,
            SpectralOscillatorFrameRenderer& renderer);
    bool renderLaneCycle(
            int laneIndex,
            const PreparedOscillatorProcessContext& context,
            const SpectralOscillatorFrameRenderer& renderer);
    void latchCurrentFrames();
    size_t blockSampleOffsetFor(
            uint64_t voiceSample,
            const PreparedOscillatorProcessContext& context) const;

    size_t maximumFrameCount {};
    int maximumCycleSamples {};
    int maximumFixedFrameSize {};
    int fixedFrameSize {};
    double sampleRate { 44100.0 };
    bool initialFramesReady {};
    double sharedFramePeriod {};
    double lastSharedFramePosition {};
    double nextSharedFramePosition {};
    uint64_t lastSharedFrameFrontier {};
    CycleDsp::UnisonVoiceLayout layout;
    std::array<LaneState, CycleDsp::maximumUnisonOrder> lanes;
    std::array<Buffer<float>, 2> currentFrames;
    std::array<Buffer<float>, 2> previousFrames;
    Buffer<float> fadeIn;
    Buffer<float> fadeOut;
    Buffer<float> biasedFrame;
    Buffer<float> shiftedCurrentFrame;
    Buffer<float> shiftedPreviousFrame;
    Buffer<float> previousHalfFrame;
    std::array<Buffer<float>, 2> resampleScratch;
    ScopedAlloc<float> laneBufferMemory;
    ScopedAlloc<float> frameMemory;
};

}
