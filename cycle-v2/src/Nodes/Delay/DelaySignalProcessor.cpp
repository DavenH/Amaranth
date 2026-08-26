#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <algorithm>

#include "Nodes/Delay/DelaySignalProcessor.h"

namespace CycleV2 {

void DelaySignalProcessor::configure(
        const DelayConfiguration& prepared,
        const AudioProcessTiming& timing) {
    bpm = std::max(1.0, timing.bpm);
    beatsPerMeasure = std::max(1, timing.beatsPerMeasure);

    CycleDsp::DelayConfiguration configuration;
    configuration.sampleRate = std::max(1.0, timing.sampleRate);
    configuration.delaySeconds = CycleDsp::delayTimeSeconds(
            prepared.time,
            bpm,
            beatsPerMeasure);
    configuration.feedback = jlimit(0.f, 0.98f, prepared.feedback);
    configuration.spin = jlimit(0.f, 1.f, prepared.spin);
    configuration.wet = jlimit(0.f, 1.f, prepared.wet);
    configuration.spinIterations = CycleDsp::delaySpinIterations(prepared.spinIterations);

    configuration.channel = CycleDsp::DelayChannel::Left;
    blockDelays[0].configure(configuration);
    traversalDelays[0].configure(configuration);
    configuration.channel = CycleDsp::DelayChannel::Right;
    blockDelays[1].configure(configuration);
    traversalDelays[1].configure(configuration);
}

void DelaySignalProcessor::beginBlock(size_t) {
    processingTraversal = false;
}

void DelaySignalProcessor::beginTraversalGrid(size_t, size_t rows) {
    ignoreUnused(rows);
    processingTraversal = true;
    traversalDelays[0].reset();
    traversalDelays[1].reset();
}

void DelaySignalProcessor::processBuffer(
        Buffer<float> buffer,
        const SignalProcessPosition& position) {
    const size_t channel = std::min<size_t>(position.channel, 1);
    auto& delay = processingTraversal
            ? traversalDelays[channel]
            : blockDelays[channel];
    delay.process(buffer);
}

}
