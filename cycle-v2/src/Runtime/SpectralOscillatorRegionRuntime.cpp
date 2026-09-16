#include "Runtime/SpectralOscillatorRegionRuntime.h"

#include "Runtime/AudioPerformanceMetrics.h"

#include <Algo/Resampling.h>
#include <Audio/CycleDsp/CyclicFrameLaneRenderer.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>
#include <Util/Arithmetic.h>

namespace CycleV2 {

bool SpectralOscillatorRegionRuntime::prepare(
        size_t maximumFrameCountToUse,
        int maximumCycleSamplesToUse,
        int maximumFixedFrameSizeToUse,
        double sampleRateToUse,
        const CycleDsp::UnisonVoiceLayout& layoutToUse,
        int controlIntervalSamplesToUse) {
    if (maximumFrameCountToUse == 0
            || maximumCycleSamplesToUse <= 0
            || maximumFixedFrameSizeToUse <= 2
            || (maximumFixedFrameSizeToUse & (maximumFixedFrameSizeToUse - 1)) != 0
            || sampleRateToUse <= 0.0
            || controlIntervalSamplesToUse <= 0
            || layoutToUse.order < 1
            || layoutToUse.order > CycleDsp::maximumUnisonOrder) {
        return false;
    }

    maximumFrameCount = maximumFrameCountToUse;
    maximumCycleSamples = maximumCycleSamplesToUse;
    maximumFixedFrameSize = maximumFixedFrameSizeToUse;
    sampleRate = sampleRateToUse;
    controlIntervalSamples = controlIntervalSamplesToUse;
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
    initialFramesReady = false;
    controlStride = 1;
    sharedFramePeriod = 0.0;
    lastSharedFramePosition = 0.0;
    nextSharedFramePosition = 0.0;
    lastSharedFrameFrontier = 0;
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        auto& lane = lanes[(size_t) laneIndex];
        lane.clock = {};
        lane.cycleCount = 0;
        lane.padding = {};
        lane.samplingSpillover = {};
        for (auto& buffer : lane.buffers) {
            buffer.reset();
        }
        for (auto lastHalf : lane.lastLerpHalf) {
            lastHalf.zero();
        }
    }
}

bool SpectralOscillatorRegionRuntime::process(
        const PreparedOscillatorProcessContext& context,
        SpectralOscillatorFrameRenderer& renderer) {
    Buffer<float> left = context.left;
    Buffer<float> right = context.right;
    if (left.size() != right.size()
            || left.empty()
            || (size_t) left.size() > maximumFrameCount
            || layout.order < 1
            || (!initialFramesReady
                    && !initializeSharedFrames(context, renderer))
            || !renderCyclesUntilReady(context, renderer)) {
        left.zero();
        right.zero();
        return false;
    }

    left.zero();
    right.zero();
    const float level = context.velocity
            * CycleDsp::UnisonCore::voiceLevelScale(layout.order);
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        const uint64_t mixStartedAt = context.performanceCounts == nullptr
                ? 0
                : AudioPerformanceMetrics::timestampMicroseconds();
        auto& lane = lanes[(size_t) laneIndex];
        float leftPan {};
        float rightPan {};
        Arithmetic::getPans(layout[laneIndex].pan, leftPan, rightPan);
        left.addProduct(lane.buffers[0].read(left.size()), level * leftPan);
        right.addProduct(lane.buffers[1].read(right.size()), level * rightPan);
        lane.buffers[0].retract();
        lane.buffers[1].retract();
        if (context.performanceCounts != nullptr) {
            context.performanceCounts->mixDurationMicroseconds
                    += AudioPerformanceMetrics::timestampMicroseconds() - mixStartedAt;
            ++context.performanceCounts->mixedLaneCount;
        }
    }
    latchCurrentFrames();
    return true;
}

bool SpectralOscillatorRegionRuntime::process(
        int midiNote,
        float velocity,
        Buffer<float> pitchEnvelope,
        Buffer<float> left,
        Buffer<float> right,
        SpectralOscillatorFrameRenderer& renderer) {
    PreparedOscillatorProcessContext context;
    context.blockFrameCount = (size_t) left.size();
    context.timing.sampleRate = sampleRate;
    context.midiNote = midiNote;
    context.velocity = velocity;
    context.pitchEnvelope = pitchEnvelope;
    context.left = left;
    context.right = right;
    return process(context, renderer);
}

int SpectralOscillatorRegionRuntime::fixedFrameSizeFor(int midiNote) const {
    const double angleDelta = CycleDsp::OscillatorLaneCore::legacyNeutralAngleDelta(
            midiNote,
            sampleRate);
    if (angleDelta <= 0.0) {
        return 0;
    }
    return Arithmetic::getNextPow2((float) (1.0 / angleDelta));
}

bool SpectralOscillatorRegionRuntime::initializeSharedFrames(
        const PreparedOscillatorProcessContext& context,
        SpectralOscillatorFrameRenderer& renderer) {
    fixedFrameSize = fixedFrameSizeFor(context.midiNote);
    if (fixedFrameSize <= 2 || fixedFrameSize > maximumFixedFrameSize) {
        return false;
    }

    const int halfSize = fixedFrameSize / 2;
    const double cyclePeriod = 1.0
            / CycleDsp::OscillatorLaneCore::legacyNeutralAngleDelta(
            context.midiNote,
            sampleRate);
    if (cyclePeriod <= 0.0) {
        fixedFrameSize = 0;
        return false;
    }
    controlStride = CycleDsp::OscillatorLaneCore::controlFrameStride(
            controlIntervalSamples,
            cyclePeriod);
    sharedFramePeriod = cyclePeriod * controlStride;
    if (!CycleDsp::CyclicFrameLaneRenderer::makeHalfFrameFades(
                fixedFrameSize,
                fadeIn.withSize(halfSize),
                fadeOut.withSize(halfSize))) {
        fixedFrameSize = 0;
        return false;
    }
    const uint64_t recipeStartedAt = context.performanceCounts == nullptr
            ? 0
            : AudioPerformanceMetrics::timestampMicroseconds();
    const bool frameRendered = renderer.renderFrame(
                    fixedFrameSize,
                    context.midiNote,
                    context,
                    context.blockSampleStart,
                    0,
                    1,
                    currentFrames[0].withSize(fixedFrameSize),
                    currentFrames[1].withSize(fixedFrameSize));
    if (context.performanceCounts != nullptr) {
        context.performanceCounts->recipeDurationMicroseconds
                += AudioPerformanceMetrics::timestampMicroseconds() - recipeStartedAt;
        ++context.performanceCounts->recipeRenderCount;
    }
    if (!frameRendered) {
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
    initialFramesReady = true;
    lastSharedFramePosition = 0.0;
    nextSharedFramePosition = sharedFramePeriod;
    lastSharedFrameFrontier = 0;
    return refreshSharedFramesThrough(
            sharedFramePeriod,
            context,
            renderer);
}

bool SpectralOscillatorRegionRuntime::refreshSharedFramesThrough(
        double cycleStart,
        const PreparedOscillatorProcessContext& context,
        SpectralOscillatorFrameRenderer& renderer) {
    while (nextSharedFramePosition <= cycleStart) {
        const uint64_t frontier = (uint64_t) nextSharedFramePosition;
        const size_t elapsedSamples = std::max<uint64_t>(
                1,
                frontier - lastSharedFrameFrontier);
        for (int channel = 0; channel < 2; ++channel) {
            currentFrames[(size_t) channel]
                    .withSize(fixedFrameSize)
                    .copyTo(previousFrames[(size_t) channel]
                            .withSize(fixedFrameSize));
        }
        const uint64_t recipeStartedAt = context.performanceCounts == nullptr
                ? 0
                : AudioPerformanceMetrics::timestampMicroseconds();
        const bool frameRendered = renderer.renderFrame(
                fixedFrameSize,
                context.midiNote,
                context,
                blockSampleOffsetFor(frontier, context),
                nextSharedFramePosition,
                elapsedSamples,
                currentFrames[0].withSize(fixedFrameSize),
                currentFrames[1].withSize(fixedFrameSize));
        if (context.performanceCounts != nullptr) {
            context.performanceCounts->recipeDurationMicroseconds
                    += AudioPerformanceMetrics::timestampMicroseconds() - recipeStartedAt;
            ++context.performanceCounts->recipeRenderCount;
        }
        if (!frameRendered) {
            return false;
        }
        lastSharedFramePosition = nextSharedFramePosition;
        lastSharedFrameFrontier = frontier;
        nextSharedFramePosition += sharedFramePeriod;
    }
    return true;
}

bool SpectralOscillatorRegionRuntime::renderCyclesUntilReady(
        const PreparedOscillatorProcessContext& context,
        SpectralOscillatorFrameRenderer& renderer) {
    if (layout.order > 1) {
        return renderMultiLaneCyclesUntilReady(context, renderer);
    }

    const int requiredSamples = context.left.size();
    const double renderHorizon = (double) context.voiceSampleStart
            + requiredSamples;
    while (true) {
        int nextLane = -1;
        double nextCycleStart = 0.0;
        for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
            const auto& lane = lanes[(size_t) laneIndex];
            if (lane.clock.cumulativePosition >= renderHorizon) {
                continue;
            }
            if (nextLane < 0 || lane.clock.cumulativePosition < nextCycleStart) {
                nextLane = laneIndex;
                nextCycleStart = lane.clock.cumulativePosition;
            }
        }
        if (nextLane < 0) {
            return true;
        }
        if (!refreshSharedFramesThrough(
                    nextCycleStart + sharedFramePeriod,
                    context,
                    renderer)
                || !renderLaneCycle(nextLane, context, renderer)) {
            return false;
        }
    }
}

bool SpectralOscillatorRegionRuntime::renderMultiLaneCyclesUntilReady(
        const PreparedOscillatorProcessContext& context,
        SpectralOscillatorFrameRenderer& renderer) {
    const double renderHorizon = (double) context.voiceSampleStart
            + context.left.size();
    while (true) {
        long minimumFrontier = lanes.front().clock.sampledFrontier;
        for (int laneIndex = 1; laneIndex < layout.order; ++laneIndex) {
            minimumFrontier = jmin(
                    minimumFrontier,
                    lanes[(size_t) laneIndex].clock.sampledFrontier);
        }
        if (minimumFrontier >= renderHorizon) {
            return true;
        }

        const auto& primaryLane = lanes.front();
        if (CycleDsp::OscillatorLaneCore::sharedFrameSaturated(
                    false,
                    primaryLane.cycleCount,
                    0,
                    primaryLane.clock.cumulativePosition,
                    lastSharedFramePosition)
                && !refreshSharedFramesThrough(
                        nextSharedFramePosition,
                        context,
                        renderer)) {
            return false;
        }

        for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
            auto& lane = lanes[(size_t) laneIndex];
            while (lane.clock.sampledFrontier < renderHorizon
                    && CycleDsp::OscillatorLaneCore::laneWithinSharedFrame(
                            false,
                            lane.cycleCount,
                            0,
                            lane.clock.cumulativePosition,
                            lastSharedFramePosition)) {
                if (!renderLaneCycle(laneIndex, context, renderer)) {
                    return false;
                }
            }
        }
    }
}

bool SpectralOscillatorRegionRuntime::renderLaneCycle(
        int laneIndex,
        const PreparedOscillatorProcessContext& context,
        const SpectralOscillatorFrameRenderer& renderer) {
    const uint64_t laneStartedAt = context.performanceCounts == nullptr
            ? 0
            : AudioPerformanceMetrics::timestampMicroseconds();
    auto& lane = lanes[(size_t) laneIndex];
    const double cycleStartPosition = lane.clock.cumulativePosition;
    const uint64_t cycleStart = (uint64_t) cycleStartPosition;
    const long relativeFrontier = lane.clock.sampledFrontier
            - lane.buffers[0].totalSamplesRead;
    const int pitchIndex = context.pitchEnvelope.empty()
            ? 0
            : jlimit(0, context.pitchEnvelope.size() - 1, (int) relativeFrontier);
    float pitch = 0.5f;
    if (renderer.hasPitchEnvelope()) {
        pitch = renderer.pitchEnvelopeValue(laneIndex);
    } else if (!context.pitchEnvelope.empty()) {
        pitch = context.pitchEnvelope[pitchIndex];
    }
    const double angleDelta = CycleDsp::OscillatorLaneCore::angleDeltaForPitchUnit(
            context.midiNote,
            layout[laneIndex].detuneCents,
            pitch,
            sampleRate);
    const bool firstCycle = (int) lane.clock.cumulativePosition == 0;
    CycleDsp::OscillatorLaneCore::advanceChainedCycle(lane.clock, angleDelta);
    if (lane.clock.samplesThisCycle <= 0
            || lane.clock.samplesThisCycle > maximumCycleSamples) {
        return false;
    }

    const float framePortion =
            CycleDsp::OscillatorLaneCore::interpolatedFramePortion(
                    layout.order == 1,
                    lane.cycleCount,
                    controlStride,
                    lastSharedFramePosition,
                    cycleStartPosition,
                    sharedFramePeriod);
    const double sourceToDestRatio = fixedFrameSize * angleDelta;
    for (int channel = 0; channel < 2; ++channel) {
        auto composed = CycleDsp::CyclicFrameLaneRenderer::compose(
                {
                        currentFrames[(size_t) channel].withSize(fixedFrameSize),
                        previousFrames[(size_t) channel].withSize(fixedFrameSize),
                        fadeIn.withSize(fixedFrameSize / 2),
                        fadeOut.withSize(fixedFrameSize / 2),
                        layout[laneIndex].phaseCycles,
                        framePortion,
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
        if (context.voice != nullptr
                && context.voice->spectralStageCapture != nullptr
                && renderer.frameRenderCount() > 0) {
            context.voice->spectralStageCapture->capture({
                    CycleDsp::SpectralStage::PitchClockedCycle,
                    renderer.frameRenderCount() - 1,
                    cycleStart,
                    context.midiNote,
                    channel,
                    output,
                    composed,
                    laneIndex
            });
        }
        lane.buffers[(size_t) channel].write(output);
    }
    ++lane.cycleCount;
    if (context.performanceCounts != nullptr) {
        context.performanceCounts->laneDurationMicroseconds
                += AudioPerformanceMetrics::timestampMicroseconds() - laneStartedAt;
        ++context.performanceCounts->laneCycleCount;
    }
    return true;
}

void SpectralOscillatorRegionRuntime::latchCurrentFrames() {
    for (int channel = 0; channel < 2; ++channel) {
        currentFrames[(size_t) channel]
                .withSize(fixedFrameSize)
                .copyTo(previousFrames[(size_t) channel]
                        .withSize(fixedFrameSize));
    }
}

size_t SpectralOscillatorRegionRuntime::blockSampleOffsetFor(
        uint64_t voiceSample,
        const PreparedOscillatorProcessContext& context) const {
    if (voiceSample <= context.voiceSampleStart) {
        return context.blockSampleStart;
    }
    return context.blockSampleStart
            + (size_t) (voiceSample - context.voiceSampleStart);
}

}
