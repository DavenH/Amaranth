#include "SpectralOscillatorRegionRuntime.h"

#include <Algo/Resampling.h>
#include <Audio/CycleDsp/CyclicFrameLaneRenderer.h>
#include <Util/Arithmetic.h>

namespace CycleV2 {

bool SpectralOscillatorRegionRuntime::prepare(
        size_t maximumFrameCountToUse,
        int maximumCycleSamplesToUse,
        int maximumFixedFrameSizeToUse,
        double sampleRateToUse,
        const CycleDsp::UnisonVoiceLayout& layoutToUse) {
    if (maximumFrameCountToUse == 0
            || maximumCycleSamplesToUse <= 0
            || maximumFixedFrameSizeToUse <= 2
            || (maximumFixedFrameSizeToUse & (maximumFixedFrameSizeToUse - 1)) != 0
            || sampleRateToUse <= 0.0
            || layoutToUse.order < 1
            || layoutToUse.order > CycleDsp::maximumUnisonOrder) {
        return false;
    }

    maximumFrameCount = maximumFrameCountToUse;
    maximumCycleSamples = maximumCycleSamplesToUse;
    maximumFixedFrameSize = maximumFixedFrameSizeToUse;
    sampleRate = sampleRateToUse;
    layout = layoutToUse;

    const int laneBufferSize = (int) maximumFrameCount + maximumCycleSamples + 1;
    laneBufferMemory.resize(layout.order * 2 * laneBufferSize);
    laneBufferMemory.resetPlacement();
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        for (auto& buffer : lanes[(size_t) laneIndex].buffers) {
            buffer.setMemoryBuffer(laneBufferMemory.place(laneBufferSize));
        }
    }

    const int maximumHalfSize = maximumFixedFrameSize / 2;
    const int sharedFrameValues = 2 * maximumFixedFrameSize
            + 2 * maximumFixedFrameSize
            + 2 * maximumHalfSize
            + 3 * maximumFixedFrameSize
            + maximumHalfSize
            + layout.order * 2 * maximumHalfSize
            + 2 * maximumCycleSamples;
    frameMemory.resize(sharedFrameValues);
    frameMemory.resetPlacement();
    for (int channel = 0; channel < 2; ++channel) {
        currentFrames[(size_t) channel] = frameMemory.place(maximumFixedFrameSize);
        previousFrames[(size_t) channel] = frameMemory.place(maximumFixedFrameSize);
    }
    fadeIn = frameMemory.place(maximumHalfSize);
    fadeOut = frameMemory.place(maximumHalfSize);
    biasedFrame = frameMemory.place(maximumFixedFrameSize);
    shiftedCurrentFrame = frameMemory.place(maximumFixedFrameSize);
    shiftedPreviousFrame = frameMemory.place(maximumFixedFrameSize);
    previousHalfFrame = frameMemory.place(maximumHalfSize);
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        for (int channel = 0; channel < 2; ++channel) {
            lanes[(size_t) laneIndex].lastLerpHalf[(size_t) channel]
                    = frameMemory.place(maximumHalfSize);
        }
    }
    for (auto& scratch : resampleScratch) {
        scratch = frameMemory.place(maximumCycleSamples);
    }
    reset();
    return true;
}

void SpectralOscillatorRegionRuntime::reset() {
    fixedFrameSize = 0;
    frameReady = false;
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        auto& lane = lanes[(size_t) laneIndex];
        lane.clock = {};
        lane.padding = {};
        lane.samplingSpillover = {};
        for (auto& buffer : lane.buffers) {
            buffer.reset();
            buffer.write(0.f);
        }
        for (auto lastHalf : lane.lastLerpHalf) {
            lastHalf.zero();
        }
    }
}

bool SpectralOscillatorRegionRuntime::process(
        int midiNote,
        float velocity,
        Buffer<float> pitchEnvelope,
        Buffer<float> left,
        Buffer<float> right,
        SpectralOscillatorFrameRenderer& renderer) {
    if (left.size() != right.size()
            || left.empty()
            || (size_t) left.size() > maximumFrameCount
            || layout.order < 1
            || (!frameReady && !renderSharedFrame(midiNote, renderer))) {
        left.zero();
        right.zero();
        return false;
    }

    left.zero();
    right.zero();
    const float level = velocity * CycleDsp::UnisonCore::voiceLevelScale(layout.order);
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        if (!renderUntilReady(
                laneIndex,
                midiNote,
                pitchEnvelope,
                (size_t) left.size())) {
            left.zero();
            right.zero();
            return false;
        }

        auto& lane = lanes[(size_t) laneIndex];
        float leftPan {};
        float rightPan {};
        Arithmetic::getPans(layout[laneIndex].pan, leftPan, rightPan);
        left.addProduct(lane.buffers[0].read(left.size()), level * leftPan);
        right.addProduct(lane.buffers[1].read(right.size()), level * rightPan);
        lane.buffers[0].retract();
        lane.buffers[1].retract();
    }
    return true;
}

int SpectralOscillatorRegionRuntime::fixedFrameSizeFor(int midiNote) const {
    const double angleDelta = CycleDsp::OscillatorLaneCore::angleDelta(
            midiNote,
            0.f,
            sampleRate);
    if (angleDelta <= 0.0) {
        return 0;
    }
    return Arithmetic::getNextPow2((float) (1.0 / angleDelta));
}

bool SpectralOscillatorRegionRuntime::renderSharedFrame(
        int midiNote,
        SpectralOscillatorFrameRenderer& renderer) {
    fixedFrameSize = fixedFrameSizeFor(midiNote);
    if (fixedFrameSize <= 2 || fixedFrameSize > maximumFixedFrameSize) {
        return false;
    }

    const int halfSize = fixedFrameSize / 2;
    if (!CycleDsp::CyclicFrameLaneRenderer::makeHalfFrameFades(
                fixedFrameSize,
                fadeIn.withSize(halfSize),
                fadeOut.withSize(halfSize))
            || !renderer.renderFrame(
                    fixedFrameSize,
                    currentFrames[0].withSize(fixedFrameSize),
                    currentFrames[1].withSize(fixedFrameSize))) {
        fixedFrameSize = 0;
        return false;
    }

    for (int channel = 0; channel < 2; ++channel) {
        currentFrames[(size_t) channel]
                .withSize(fixedFrameSize)
                .copyTo(previousFrames[(size_t) channel].withSize(fixedFrameSize));
    }
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        for (int channel = 0; channel < 2; ++channel) {
            previousFrames[(size_t) channel]
                    .withSize(halfSize)
                    .copyTo(lanes[(size_t) laneIndex]
                                    .lastLerpHalf[(size_t) channel]
                                    .withSize(halfSize));
        }
    }
    frameReady = true;
    return true;
}

bool SpectralOscillatorRegionRuntime::renderUntilReady(
        int laneIndex,
        int midiNote,
        Buffer<float> pitchEnvelope,
        size_t frameCount) {
    auto& lane = lanes[(size_t) laneIndex];
    while (!lane.buffers[0].hasDataFor((int) frameCount)) {
        const long relativeFrontier = lane.clock.sampledFrontier
                - lane.buffers[0].totalSamplesRead;
        const int pitchIndex = pitchEnvelope.empty()
                ? 0
                : jlimit(0, pitchEnvelope.size() - 1, (int) relativeFrontier);
        const float pitch = pitchEnvelope.empty() ? 0.5f : pitchEnvelope[pitchIndex];
        const double angleDelta = CycleDsp::OscillatorLaneCore::angleDeltaForPitchUnit(
                midiNote,
                layout[laneIndex].detuneCents,
                pitch,
                sampleRate);
        const bool firstCycle = (int) lane.clock.cumulativePosition == 0;
        CycleDsp::OscillatorLaneCore::advanceChainedCycle(lane.clock, angleDelta);
        if (lane.clock.samplesThisCycle <= 0
                || lane.clock.samplesThisCycle > maximumCycleSamples) {
            return false;
        }

        const double sourceToDestRatio = fixedFrameSize * angleDelta;
        for (int channel = 0; channel < 2; ++channel) {
            auto composed = CycleDsp::CyclicFrameLaneRenderer::compose(
                    {
                            currentFrames[(size_t) channel].withSize(fixedFrameSize),
                            previousFrames[(size_t) channel].withSize(fixedFrameSize),
                            fadeIn.withSize(fixedFrameSize / 2),
                            fadeOut.withSize(fixedFrameSize / 2),
                            layout[laneIndex].phaseCycles,
                            0.f,
                            firstCycle,
                            layout.order > 1
                    },
                    {
                            lane.lastLerpHalf[(size_t) channel]
                                    .withSize(fixedFrameSize / 2)
                    },
                    {
                            biasedFrame.withSize(fixedFrameSize),
                            shiftedCurrentFrame.withSize(fixedFrameSize),
                            shiftedPreviousFrame.withSize(fixedFrameSize),
                            previousHalfFrame.withSize(fixedFrameSize / 2)
                    });
            if (composed.empty()) {
                return false;
            }

            auto output = resampleScratch[(size_t) channel]
                    .withSize(lane.clock.samplesThisCycle);
            auto& padding = lane.padding[(size_t) channel];
            Resampling::resample(
                    composed,
                    output,
                    sourceToDestRatio,
                    padding[0],
                    padding[1],
                    padding[2],
                    padding[3],
                    padding[4],
                    padding[5],
                    padding[6],
                    lane.samplingSpillover[(size_t) channel],
                    Resampling::Hermite);
            lane.buffers[(size_t) channel].write(output);
        }
    }
    return lane.buffers[1].hasDataFor((int) frameCount);
}

}
