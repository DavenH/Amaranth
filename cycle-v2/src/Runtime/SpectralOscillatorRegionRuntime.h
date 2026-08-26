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
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right,
            SpectralOscillatorFrameRenderer& renderer);

private:
    struct LaneState {
        CycleDsp::ChainedCycleState clock;
        std::array<ReadWriteBuffer, 2> buffers;
        std::array<std::array<float, 7>, 2> padding {};
        std::array<double, 2> samplingSpillover {};
        std::array<Buffer<float>, 2> lastLerpHalf;
    };

    int fixedFrameSizeFor(int midiNote) const;
    bool renderSharedFrame(
            int midiNote,
            SpectralOscillatorFrameRenderer& renderer);
    bool renderUntilReady(
            int laneIndex,
            int midiNote,
            Buffer<float> pitchEnvelope,
            size_t frameCount);

    size_t maximumFrameCount {};
    int maximumCycleSamples {};
    int maximumFixedFrameSize {};
    int fixedFrameSize {};
    double sampleRate { 44100.0 };
    bool frameReady {};
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
