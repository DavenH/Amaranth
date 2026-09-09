#include <algorithm>
#include <cmath>

#include <Array/Buffer.h>
#include <Audio/CycleDsp/InternalRateBlockAdapter.h>

#include "Runtime/OfflineGraphAudioRenderer.h"
#include "Runtime/LegacyOutputRateAdapter.h"
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

bool enqueueBlockEvents(
        const std::vector<OfflineGraphAudioEvent>& events,
        size_t& eventIndex,
        size_t outputStart,
        int outputFrameCount,
        size_t internalStart,
        const LegacyOutputRateAdapter& rateAdapter,
        RealtimeMidiEventQueue& queue) {
    const size_t outputEnd = outputStart + (size_t) outputFrameCount;
    while (eventIndex < events.size() && events[eventIndex].sampleOffset < outputStart) {
        ++eventIndex;
    }
    while (eventIndex < events.size() && events[eventIndex].sampleOffset < outputEnd) {
        const auto& event = events[eventIndex];
        const int relativeOutputSample = (int) (event.sampleOffset - outputStart);
        const size_t internalSample = internalStart
                + (size_t) rateAdapter.convertSampleOffset(relativeOutputSample);
        if (!queue.enqueue(
                event.message,
                event.source,
                (double) internalSample
                        / CycleDsp::InternalRateBlockAdapter::internalSampleRate)) {
            return false;
        }
        ++eventIndex;
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

    const bool convertsOutputRate = request.ratePolicy
                    == OfflineGraphAudioRatePolicy::LegacyInternal44100
            && request.sampleRate
                    != CycleDsp::InternalRateBlockAdapter::internalSampleRate;
    const double renderSampleRate = convertsOutputRate
            ? CycleDsp::InternalRateBlockAdapter::internalSampleRate
            : request.sampleRate;
    const size_t maximumRenderFrameCount = convertsOutputRate
            ? (size_t) std::ceil(
                    request.blockSize * renderSampleRate / request.sampleRate) + 1
            : (size_t) request.blockSize;

    AudioExecutionSpec spec;
    spec.maximumFrameCount = maximumRenderFrameCount;
    spec.sampleRate = renderSampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(
            std::move(plan),
            revision,
            spec);
    if (prepared == nullptr) {
        result.error = "Graph preparation failed";
        return result;
    }

    RealtimeMidiEventQueue queue;
    if (!convertsOutputRate && !enqueueEvents(request, queue)) {
        result.error = "MIDI schedule contains an unsupported event";
        return result;
    }

    RealtimeGraphRenderer renderer;
    renderer.setPreparedGraph(prepared.get());
    renderer.setSpectralStageCapture(request.spectralStageCapture);
    renderer.setVoiceDurationSeconds(request.voiceDurationSeconds);
    renderer.setOutputGain(request.outputGain);
    renderer.setControlNoteOffset(request.controlNoteOffset);
    for (int channel = 0; channel < request.channelCount; ++channel) {
        result.channels[(size_t) channel].resize(request.sampleCount);
    }
    std::array<std::vector<float>, 2> blocks;
    std::array<std::vector<float>, 2> renderBlocks;
    std::array<float*, 2> blockChannels {};
    std::array<float*, 2> renderBlockChannels {};
    for (int channel = 0; channel < request.channelCount; ++channel) {
        blocks[(size_t) channel].resize((size_t) request.blockSize);
        blockChannels[(size_t) channel] = blocks[(size_t) channel].data();
        renderBlocks[(size_t) channel].resize(maximumRenderFrameCount);
        renderBlockChannels[(size_t) channel] = renderBlocks[(size_t) channel].data();
    }

    LegacyOutputRateAdapter rateAdapter;
    std::vector<OfflineGraphAudioEvent> sortedEvents = request.events;
    size_t eventIndex {};
    size_t internalStart {};
    if (convertsOutputRate) {
        rateAdapter.prepare(
                request.sampleRate,
                request.blockSize,
                request.channelCount);
        std::stable_sort(
                sortedEvents.begin(),
                sortedEvents.end(),
                [](const auto& left, const auto& right) {
                    return left.sampleOffset < right.sampleOffset;
                });
    }

    for (size_t start = 0; start < request.sampleCount; start += request.blockSize) {
        const int frameCount = (int) std::min(
                (size_t) request.blockSize,
                request.sampleCount - start);
        if (convertsOutputRate) {
            const int renderFrameCount = rateAdapter.convertBlockSize(frameCount);
            if (!enqueueBlockEvents(
                    sortedEvents,
                    eventIndex,
                    start,
                    frameCount,
                    internalStart,
                    rateAdapter,
                    queue)) {
                result.error = "MIDI schedule contains an unsupported event";
                return result;
            }
            if (renderFrameCount > 0) {
                renderer.process(
                        queue,
                        renderBlockChannels.data(),
                        request.channelCount,
                        renderFrameCount,
                        renderSampleRate,
                        (double) internalStart / renderSampleRate);
            }
            if (!rateAdapter.convertAudio(
                    renderBlocks,
                    renderFrameCount,
                    blockChannels.data(),
                    request.channelCount,
                    frameCount)) {
                result.error = "Legacy output-rate conversion underflowed";
                return result;
            }
            internalStart += (size_t) renderFrameCount;
        } else {
            renderer.process(
                    queue,
                    blockChannels.data(),
                    request.channelCount,
                    frameCount,
                    request.sampleRate,
                    (double) start / request.sampleRate);
        }
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
