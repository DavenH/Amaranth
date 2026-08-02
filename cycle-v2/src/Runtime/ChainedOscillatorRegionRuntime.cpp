#include "ChainedOscillatorRegionRuntime.h"

#include <Util/Arithmetic.h>

namespace CycleV2 {

bool ChainedOscillatorRegionRuntime::prepare(
        size_t maximumFrameCountToUse,
        int maximumCycleSamplesToUse,
        double sampleRateToUse,
        const CycleDsp::UnisonVoiceLayout& layoutToUse) {
    if (maximumFrameCountToUse == 0
            || maximumCycleSamplesToUse <= 0
            || sampleRateToUse <= 0.0
            || layoutToUse.order < 1
            || layoutToUse.order > CycleDsp::maximumUnisonOrder) {
        return false;
    }

    maximumFrameCount = maximumFrameCountToUse;
    maximumCycleSamples = maximumCycleSamplesToUse;
    sampleRate = sampleRateToUse;
    layout = layoutToUse;
    const int laneBufferSize = (int) maximumFrameCountToUse + maximumCycleSamples + 1;
    laneBufferMemory.resize(
            layout.order * 2 * laneBufferSize);
    scratchMemory.resize(2 * maximumCycleSamples);
    laneBufferMemory.resetPlacement();

    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        for (auto& buffer : lanes[(size_t) laneIndex].buffers) {
            buffer.setMemoryBuffer(laneBufferMemory.place(laneBufferSize));
        }
    }
    reset();
    return true;
}

void ChainedOscillatorRegionRuntime::reset() {
    for (int laneIndex = 0; laneIndex < layout.order; ++laneIndex) {
        auto& lane = lanes[(size_t) laneIndex];
        lane.clock = {};
        for (auto& buffer : lane.buffers) {
            buffer.reset();
            buffer.write(0.f);
        }
    }
}

bool ChainedOscillatorRegionRuntime::process(
        int midiNote,
        float velocity,
        Buffer<float> pitchEnvelope,
        Buffer<float> left,
        Buffer<float> right,
        OscillatorCycleRenderer& renderer) {
    if (left.size() != right.size()
            || left.empty()
            || (size_t) left.size() > maximumFrameCount
            || layout.order < 1) {
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
                (size_t) left.size(),
                renderer)) {
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

bool ChainedOscillatorRegionRuntime::renderUntilReady(
        int laneIndex,
        int midiNote,
        Buffer<float> pitchEnvelope,
        size_t frameCount,
        OscillatorCycleRenderer& renderer) {
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
        const double cycleStart = lane.clock.cumulativePosition;
        CycleDsp::OscillatorLaneCore::advanceChainedCycle(lane.clock, angleDelta);
        if (lane.clock.samplesThisCycle <= 0
                || lane.clock.samplesThisCycle > maximumCycleSamples) {
            return false;
        }

        Buffer<float> cycleLeft(
                scratchMemory.get(),
                lane.clock.samplesThisCycle);
        Buffer<float> cycleRight(
                scratchMemory.get() + maximumCycleSamples,
                lane.clock.samplesThisCycle);
        cycleLeft.zero();
        cycleRight.zero();
        renderer.renderCycle({
                laneIndex,
                lane.clock.samplesThisCycle,
                angleDelta,
                cycleStart,
                layout[laneIndex]
        }, cycleLeft, cycleRight);
        lane.buffers[0].write(cycleLeft);
        lane.buffers[1].write(cycleRight);
    }
    return lane.buffers[1].hasDataFor((int) frameCount);
}

}
