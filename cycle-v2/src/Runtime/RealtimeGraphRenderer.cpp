#include "RealtimeGraphRenderer.h"

#include <Array/Buffer.h>

#include <algorithm>
#include <cmath>

namespace CycleV2 {

using namespace juce;

RealtimeGraphRenderer::RealtimeGraphRenderer() {
    midiControls.prepare(maximumEventsPerChannel);
    for (auto& voice : voices) {
        voice.context.events.reserve(RealtimeMidiEventQueue::capacity * 2);
        midiControls.prepareVoice(voice.context);
    }
}

std::unique_ptr<RealtimeGraphRenderer::PreparedGraph>
RealtimeGraphRenderer::prepareGraph(
        GraphExecutionPlan plan,
        uint64_t revision,
        const AudioExecutionSpec& spec) {
    auto prepared = std::make_unique<PreparedGraph>();
    prepared->revision = revision;
    prepared->plan = std::move(plan);
    prepared->spec = spec;
    for (size_t voiceIndex = 0; voiceIndex < voiceCount; ++voiceIndex) {
        prepared->executor.prepareExecution(
                prepared->plan,
                prepared->spec,
                (int) voiceIndex);
    }
    return prepared;
}

void RealtimeGraphRenderer::setPreparedGraph(PreparedGraph* graph) {
    if (preparedGraph == graph) {
        return;
    }
    resetVoices();
    preparedGraph = graph;
    activeRevision.store(graph == nullptr ? 0 : graph->revision, std::memory_order_release);
}

void RealtimeGraphRenderer::process(
        RealtimeMidiEventQueue& events,
        float* const* outputChannels,
        int outputChannelCount,
        int frameCount,
        double sampleRate,
        double callbackStartSeconds) {
    for (int channel = 0; channel < outputChannelCount; ++channel) {
        if (outputChannels[channel] != nullptr) {
            Buffer<float>(outputChannels[channel], frameCount).zero();
        }
    }

    callbackCounter.fetch_add(1, std::memory_order_relaxed);
    if (preparedGraph == nullptr
            || frameCount <= 0
            || (size_t) frameCount > preparedGraph->spec.maximumFrameCount) {
        activeVoices.store(0, std::memory_order_relaxed);
        outputPeak.store(0.f, std::memory_order_relaxed);
        outputRms.store(0.f, std::memory_order_relaxed);
        return;
    }

    beginBlock();
    consumeEvents(events, frameCount, sampleRate, callbackStartSeconds);
    renderVoices(outputChannels, outputChannelCount, frameCount, sampleRate);
    publishMetrics(outputChannels, outputChannelCount, frameCount);
}

void RealtimeGraphRenderer::resetVoices() {
    for (auto& voice : voices) {
        voice.context.events.clear();
        voice.context.controlEvents.clear();
        voice.active = false;
        voice.released = false;
        voice.normalizedTime = 0.f;
    }
    scheduledEventCount = 0;
    midiControls.reset();
    activeVoices.store(0, std::memory_order_relaxed);
}

void RealtimeGraphRenderer::beginBlock() {
    midiControls.beginBlock();
    for (auto& voice : voices) {
        voice.context.events.clear();
        voice.context.controlEvents.clear();
    }
}

void RealtimeGraphRenderer::consumeEvents(
        RealtimeMidiEventQueue& queue,
        int frameCount,
        double sampleRate,
        double callbackStartSeconds) {
    if (queue.consumeRecoveryRequest(MidiEventSource::PerformanceKeyboard)) {
        releaseSource(MidiEventSource::PerformanceKeyboard, 0);
    }
    if (queue.consumeRecoveryRequest(MidiEventSource::Hardware)) {
        releaseSource(MidiEventSource::Hardware, 0);
    }

    RealtimeMidiEvent event;
    while (scheduledEventCount < scheduledEvents.size() && queue.dequeue(event)) {
        scheduledEvents[scheduledEventCount++] = event;
    }

    std::sort(
            scheduledEvents.begin(),
            scheduledEvents.begin() + scheduledEventCount,
            [](const auto& left, const auto& right) {
                if (left.timestampSeconds == right.timestampSeconds) {
                    return left.sequence < right.sequence;
                }
                return left.timestampSeconds < right.timestampSeconds;
            });

    const double callbackEndSeconds = callbackStartSeconds
            + (double) frameCount / sampleRate;
    size_t consumedCount {};
    while (consumedCount < scheduledEventCount
            && scheduledEvents[consumedCount].timestampSeconds < callbackEndSeconds) {
        const RealtimeMidiEvent& scheduled = scheduledEvents[consumedCount];
        const double relativeSamples = (scheduled.timestampSeconds - callbackStartSeconds) * sampleRate;
        const size_t sampleOffset = (size_t) jlimit(
                0,
                frameCount - 1,
                roundToInt(relativeSamples));
        applyEvent(scheduled, sampleOffset);
        ++consumedCount;
    }

    if (consumedCount > 0) {
        std::move(
                scheduledEvents.begin() + consumedCount,
                scheduledEvents.begin() + scheduledEventCount,
                scheduledEvents.begin());
        scheduledEventCount -= consumedCount;
    }
}

void RealtimeGraphRenderer::applyEvent(
        const RealtimeMidiEvent& event,
        size_t sampleOffset) {
    switch (event.kind) {
        case RealtimeMidiEvent::Kind::NoteOn:
            allocateVoice(event, sampleOffset);
            return;

        case RealtimeMidiEvent::Kind::NoteOff: {
            Voice* oldestMatchingVoice {};
            for (auto& voice : voices) {
                if (voice.active
                        && !voice.released
                        && voice.source == event.source
                        && voice.midiChannel == event.channel
                        && voice.noteNumber == event.data1) {
                    if (oldestMatchingVoice == nullptr
                            || voice.startOrder < oldestMatchingVoice->startOrder) {
                        oldestMatchingVoice = &voice;
                    }
                }
            }
            if (oldestMatchingVoice != nullptr) {
                const bool hasTail = preparedGraph->executor.hasVoiceTailProcessor(
                        oldestMatchingVoice->context.voiceIndex);
                oldestMatchingVoice->context.events.push_back({
                        hasTail ? NoteLifecycleType::NoteOff : NoteLifecycleType::Reset,
                        sampleOffset,
                        oldestMatchingVoice->context.voiceIndex
                });
                oldestMatchingVoice->released = true;
            }
            return;
        }

        case RealtimeMidiEvent::Kind::Controller:
        case RealtimeMidiEvent::Kind::ChannelPressure:
            midiControls.ingest(event.toMidiMessage(), sampleOffset);
            return;

        case RealtimeMidiEvent::Kind::AllNotesOff:
            releaseSource(event.source, sampleOffset);
            return;
    }
}

void RealtimeGraphRenderer::releaseSource(
        MidiEventSource source,
        size_t sampleOffset) {
    for (auto& voice : voices) {
        if (!voice.active || voice.released || voice.source != source) {
            continue;
        }
        const bool hasTail = preparedGraph != nullptr
                && preparedGraph->executor.hasVoiceTailProcessor(voice.context.voiceIndex);
        voice.context.events.push_back({
                hasTail ? NoteLifecycleType::NoteOff : NoteLifecycleType::Reset,
                sampleOffset,
                voice.context.voiceIndex
        });
        voice.released = true;
    }
}

RealtimeGraphRenderer::Voice& RealtimeGraphRenderer::allocateVoice(
        const RealtimeMidiEvent& event,
        size_t sampleOffset) {
    auto selected = std::find_if(voices.begin(), voices.end(), [](const auto& voice) {
        return !voice.active;
    });
    if (selected == voices.end()) {
        selected = std::min_element(voices.begin(), voices.end(), [](const auto& left, const auto& right) {
            return left.startOrder < right.startOrder;
        });
        selected->context.events.push_back({
                NoteLifecycleType::Reset,
                sampleOffset,
                selected->context.voiceIndex
        });
    }

    const int voiceIndex = (int) std::distance(voices.begin(), selected);
    selected->context.voiceIndex = voiceIndex;
    selected->context.events.push_back({ NoteLifecycleType::NoteOn, sampleOffset, voiceIndex });
    selected->source = event.source;
    selected->startOrder = ++nextVoiceOrder;
    selected->midiChannel = event.channel;
    selected->noteNumber = event.data1;
    selected->velocity = (float) event.data2 / 127.f;
    selected->normalizedTime = 0.f;
    selected->active = true;
    selected->released = false;
    return *selected;
}

void RealtimeGraphRenderer::renderVoices(
        float* const* outputChannels,
        int outputChannelCount,
        int frameCount,
        double sampleRate) {
    size_t activeCount = 0;
    constexpr float voiceDurationSeconds = 7.f;
    const float timeIncrement = sampleRate > 0.
            ? 1.f / ((float) sampleRate * voiceDurationSeconds)
            : 0.f;

    for (auto& voice : voices) {
        if (!voice.active) {
            continue;
        }
        voice.context.controls.noteNumber = voice.noteNumber;
        voice.context.controls.velocity = voice.velocity;
        voice.context.controls.normalizedVoiceTime = voice.normalizedTime;
        voice.context.controls.normalizedVoiceTimeIncrement = timeIncrement;
        midiControls.populateVoice(voice.context, voice.midiChannel);

        const auto output = preparedGraph->executor.processRealtime(
                preparedGraph->plan,
                (size_t) frameCount,
                { sampleRate },
                voice.context);
        if (output.isValid() && output.payload != nullptr && outputChannelCount > 0) {
            const auto& payload = *output.payload;
            if (outputChannels[0] != nullptr) {
                Buffer<float>(outputChannels[0], frameCount).add(
                        Buffer<float>(
                                const_cast<float*>(payload.block.samples.data()),
                                frameCount));
            }
            if (outputChannelCount > 1 && outputChannels[1] != nullptr) {
                const SignalBuffer& right = payload.isStereo()
                        ? payload.secondaryBlock.samples
                        : payload.block.samples;
                Buffer<float>(outputChannels[1], frameCount).add(
                        Buffer<float>(const_cast<float*>(right.data()), frameCount));
            }
        }

        voice.normalizedTime = jmin(
                1.f,
                voice.normalizedTime + timeIncrement * (float) frameCount);
        if (voice.released && !preparedGraph->executor.hasActiveVoiceTail(
                voice.context.voiceIndex)) {
            voice.active = false;
        }
        if (voice.active) {
            ++activeCount;
        }
    }

    for (int channel = 0; channel < jmin(2, outputChannelCount); ++channel) {
        if (outputChannels[channel] != nullptr) {
            Buffer<float>(outputChannels[channel], frameCount)
                    .mul(outputHeadroom)
                    .clip(-1.f, 1.f);
        }
    }
    activeVoices.store(activeCount, std::memory_order_relaxed);
}

void RealtimeGraphRenderer::publishMetrics(
        float* const* outputChannels,
        int outputChannelCount,
        int frameCount) {
    float peak {};
    double squaredNorm {};
    int measuredChannels {};
    for (int channel = 0; channel < jmin(2, outputChannelCount); ++channel) {
        if (outputChannels[channel] == nullptr) {
            continue;
        }
        Buffer<float> output(outputChannels[channel], frameCount);
        const float norm = output.normL2();
        const float magnitude = output.normL1();
        if (!std::isfinite(norm) || !std::isfinite(magnitude)) {
            output.zero();
            continue;
        }
        if ((size_t) frameCount <= metricsScratch.size()) {
            Buffer<float> absolute(metricsScratch.data(), frameCount);
            output.copyTo(absolute);
            absolute.abs();
            peak = jmax(peak, absolute.max());
        }
        squaredNorm += (double) norm * (double) norm;
        ++measuredChannels;
    }

    const double denominator = (double) jmax(1, measuredChannels * frameCount);
    outputPeak.store(peak, std::memory_order_relaxed);
    outputRms.store((float) std::sqrt(squaredNorm / denominator), std::memory_order_relaxed);
}

}
