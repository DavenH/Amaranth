#include "Runtime/ChainedOscillatorRegionRuntime.h"

#include "Runtime/PreparedOscillatorRegion.h"

#include <Util/Arithmetic.h>

#include <algorithm>

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
        const PreparedOscillatorProcessContext& context,
        OscillatorCycleRenderer& renderer) {
    const int midiNote = context.midiNote;
    const float velocity = context.velocity;
    const Buffer<float> pitchEnvelope = context.pitchEnvelope;
    Buffer<float> left = context.left;
    Buffer<float> right = context.right;
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
        if (!renderUntilReady(laneIndex, context, renderer)) {
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

bool ChainedOscillatorRegionRuntime::process(
        int midiNote,
        float velocity,
        Buffer<float> pitchEnvelope,
        Buffer<float> left,
        Buffer<float> right,
        OscillatorCycleRenderer& renderer) {
    PreparedOscillatorProcessContext context;
    context.timing.sampleRate = sampleRate;
    context.midiNote = midiNote;
    context.velocity = velocity;
    context.pitchEnvelope = pitchEnvelope;
    context.left = left;
    context.right = right;
    return process(context, renderer);
}

bool ChainedOscillatorRegionRuntime::renderUntilReady(
        int laneIndex,
        const PreparedOscillatorProcessContext& context,
        OscillatorCycleRenderer& renderer) {
    auto& lane = lanes[(size_t) laneIndex];
    const size_t frameCount = (size_t) context.left.size();
    while (!lane.buffers[0].hasDataFor((int) frameCount)) {
        const long relativeFrontier = lane.clock.sampledFrontier
                - lane.buffers[0].totalSamplesRead;
        const int pitchIndex = context.pitchEnvelope.empty()
                ? 0
                : jlimit(0, context.pitchEnvelope.size() - 1, (int) relativeFrontier);
        const float pitch = context.pitchEnvelope.empty()
                ? 0.5f
                : context.pitchEnvelope[pitchIndex];
        const double angleDelta = CycleDsp::OscillatorLaneCore::angleDeltaForPitchUnit(
                context.midiNote,
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
                layout[laneIndex],
                &context,
                context.blockSampleStart + (size_t) std::max<double>(
                        0.0,
                        cycleStart - context.voiceSampleStart)
        }, cycleLeft, cycleRight);
        lane.buffers[0].write(cycleLeft);
        lane.buffers[1].write(cycleRight);
    }
    return lane.buffers[1].hasDataFor((int) frameCount);
}

}
