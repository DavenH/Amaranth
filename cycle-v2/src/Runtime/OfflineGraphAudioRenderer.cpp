#include <algorithm>

#include <Array/Buffer.h>

#include "Runtime/OfflineGraphAudioRenderer.h"
#include "Runtime/RealtimeGraphRenderer.h"

namespace CycleV2 {

namespace {

juce::String validateRequest(const OfflineGraphAudioRequest& request) {
    if (request.sampleRate <= 0.0) {
        return "Sample rate must be positive";
    }
    if (request.blockSize < 1 || request.blockSize > 8192) {
        return "Block size must be between 1 and 8192 samples";
    }
    if (request.channelCount < 1 || request.channelCount > 2) {
        return "Channel count must be one or two";
    }
    if (request.voiceDurationSeconds <= 0.f) {
        return "Voice duration must be positive";
    }
    if (request.sampleCount == 0) {
        return "Sample count must be positive";
    }
    if (request.events.size() > RealtimeMidiEventQueue::capacity) {
        return "MIDI event count exceeds the realtime queue capacity";
    }
    for (const auto& event : request.events) {
        if (event.sampleOffset > request.sampleCount) {
            return "MIDI event occurs outside the render duration";
        }
    }
    return {};
}

bool enqueueEvents(
        const OfflineGraphAudioRequest& request,
        RealtimeMidiEventQueue& queue) {
    for (const auto& event : request.events) {
        const double timestamp = (double) event.sampleOffset / request.sampleRate;
        if (!queue.enqueue(event.message, event.source, timestamp)) {
            return false;
        }
    }
    return true;
}

}

OfflineGraphAudioResult OfflineGraphAudioRenderer::render(
        GraphExecutionPlan plan,
        uint64_t revision,
        const OfflineGraphAudioRequest& request) {
    OfflineGraphAudioResult result;
    result.error = validateRequest(request);
    if (result.error.isNotEmpty()) {
        return result;
    }

    AudioExecutionSpec spec;
    spec.maximumFrameCount = (size_t) request.blockSize;
    spec.sampleRate = request.sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(
            std::move(plan),
            revision,
            spec);
    if (prepared == nullptr) {
        result.error = "Graph preparation failed";
        return result;
    }

    RealtimeMidiEventQueue queue;
    if (!enqueueEvents(request, queue)) {
        result.error = "MIDI schedule contains an unsupported event";
        return result;
    }

    RealtimeGraphRenderer renderer;
    renderer.setPreparedGraph(prepared.get());
    renderer.setSpectralStageCapture(request.spectralStageCapture);
    renderer.setVoiceDurationSeconds(request.voiceDurationSeconds);
    for (int channel = 0; channel < request.channelCount; ++channel) {
        result.channels[(size_t) channel].resize(request.sampleCount);
    }
    std::array<std::vector<float>, 2> blocks;
    std::array<float*, 2> blockChannels {};
    for (int channel = 0; channel < request.channelCount; ++channel) {
        blocks[(size_t) channel].resize((size_t) request.blockSize);
        blockChannels[(size_t) channel] = blocks[(size_t) channel].data();
    }

    for (size_t start = 0; start < request.sampleCount; start += request.blockSize) {
        const int frameCount = (int) std::min(
                (size_t) request.blockSize,
                request.sampleCount - start);
        renderer.process(
                queue,
                blockChannels.data(),
                request.channelCount,
                frameCount,
                request.sampleRate,
                (double) start / request.sampleRate);
        for (int channel = 0; channel < request.channelCount; ++channel) {
            Buffer<float>(blocks[(size_t) channel].data(), frameCount).copyTo({
                    result.channels[(size_t) channel].data() + start,
                    frameCount
            });
        }
    }

    result.droppedMidiEvents = queue.droppedEventCount();
    result.succeeded = result.droppedMidiEvents == 0;
    if (!result.succeeded) {
        result.error = "MIDI events were dropped during rendering";
    }
    return result;
}

}
