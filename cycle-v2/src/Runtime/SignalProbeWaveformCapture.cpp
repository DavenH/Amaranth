#include "Runtime/SignalProbeWaveformCapture.h"

#include <cmath>

namespace CycleV2 {

namespace {

constexpr double kSampleRate = 44100.;
constexpr size_t kBlockSize = 256;

class ProbeCapture final : public GraphProcessObserver {
public:
    ProbeCapture(const GraphExecutionPlan& plan, const CompiledSignalProbe& address, size_t size) :
            nodeId(plan.steps[(size_t) address.sourceStepIndex].nodeId)
        ,   outputIndex((size_t) address.sourceOutputIndex)
        ,   samples(std::make_shared<std::vector<float>>(size, 0.f)) {}

    void nodeProcessed(const String& id, const AudioProcessContext& context) override {
        if (id != nodeId || outputIndex >= context.outputs.size()) {
            return;
        }
        const auto& payload = context.outputs[outputIndex];
        if (payload.domain != PortDomain::TimeSignal || payload.block.samples.size() < context.frameCount) {
            return;
        }
        Buffer<float> target(samples->data() + offset, (int) context.frameCount);
        Buffer<float>(const_cast<float*>(payload.block.samples.data()), target.size()).copyTo(target);
        if (payload.isStereo() && payload.secondaryBlock.samples.size() >= context.frameCount) {
            target.add(Buffer<float>(const_cast<float*>(payload.secondaryBlock.samples.data()), target.size()));
        }
        captured = true;
    }

    String nodeId;
    size_t outputIndex {};
    size_t offset {};
    bool captured {};
    std::shared_ptr<std::vector<float>> samples;
};

}

std::shared_ptr<const std::vector<float>> SignalProbeWaveformCapture::capture(
        const GraphExecutionPlan& plan,
        const CompiledSignalProbe& address,
        AudioVoiceContext voice,
        double releaseDuration,
        double noteOffTime) {
    if (address.sourceStepIndex < 0 || (size_t) address.sourceStepIndex >= plan.steps.size()
            || address.sourceOutputIndex < 0) {
        return {};
    }
    const size_t length = (size_t) std::ceil((1. + jmax(0., releaseDuration)) * kSampleRate);
    const size_t noteOff = (size_t) std::llround(jlimit(0., 1., noteOffTime) * kSampleRate);
    ProbeCapture capture(plan, address, length);
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = kBlockSize;
    spec.prewarmTraversalGrid = false;
    executor.prepareExecution(plan, spec);
    voice.controls.normalizedVoiceTimeIncrement = 1. / kSampleRate;
    voice.controls.normalizedVolumeEnvelopeTimeIncrement = 1. / kSampleRate;
    for (size_t start = 0; start < length; start += kBlockSize) {
        const size_t frames = jmin(kBlockSize, length - start);
        voice.controls.normalizedVoiceTime = (float) jmin(1., (double) start / kSampleRate);
        voice.events.clear();
        if (start == 0) {
            voice.events.push_back({ NoteLifecycleType::NoteOn, 0, voice.voiceIndex });
        }
        if (noteOff >= start && noteOff < start + frames) {
            voice.events.push_back({ NoteLifecycleType::NoteOff, noteOff - start, voice.voiceIndex });
        }
        capture.offset = start;
        executor.processRealtime(plan, frames, {}, voice, &capture);
    }
    return capture.captured ? capture.samples : nullptr;
}

}
