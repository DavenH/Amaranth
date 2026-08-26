#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <memory>

#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/MidiControlState.h"
#include "Runtime/RealtimeMidiEventQueue.h"

namespace CycleV2 {

class RealtimeGraphRenderer {
public:
    static constexpr size_t voiceCount = 8;

    struct PreparedGraph {
        uint64_t revision {};
        GraphExecutionPlan plan;
        AudioExecutionSpec spec;
        GraphAudioExecutor executor;
    };

    struct Diagnostics {
        uint64_t callbackCount {};
        uint64_t graphRevision {};
        size_t activeVoiceCount {};
        size_t droppedMidiEvents {};
        float peak {};
        float rms {};
    };

    RealtimeGraphRenderer();

    static std::unique_ptr<PreparedGraph> prepareGraph(
            GraphExecutionPlan plan,
            uint64_t revision,
            const AudioExecutionSpec& spec);
    void setPreparedGraph(PreparedGraph* graph);
    void process(
            RealtimeMidiEventQueue& events,
            float* const* outputChannels,
            int outputChannelCount,
            int frameCount,
            double sampleRate,
            double callbackStartSeconds);
    void resetVoices();
    Diagnostics diagnostics(const RealtimeMidiEventQueue& events) const {
        return {
                callbackCounter.load(std::memory_order_acquire),
                activeRevision.load(std::memory_order_acquire),
                activeVoices.load(std::memory_order_acquire),
                events.droppedEventCount(),
                outputPeak.load(std::memory_order_acquire),
                outputRms.load(std::memory_order_acquire)
        };
    }

private:
    struct Voice {
        AudioVoiceContext context;
        MidiEventSource source { MidiEventSource::PerformanceKeyboard };
        uint64_t startOrder {};
        int midiChannel { 1 };
        int noteNumber { 60 };
        float velocity { 1.f };
        float normalizedTime {};
        bool active {};
        bool released {};
    };

    void beginBlock();
    void consumeEvents(
            RealtimeMidiEventQueue& queue,
            int frameCount,
            double sampleRate,
            double callbackStartSeconds);
    void applyEvent(const RealtimeMidiEvent& event, size_t sampleOffset);
    void releaseSource(MidiEventSource source, size_t sampleOffset);
    Voice& allocateVoice(const RealtimeMidiEvent& event, size_t sampleOffset);
    void renderVoices(
            float* const* outputChannels,
            int outputChannelCount,
            int frameCount,
            double sampleRate);
    void publishMetrics(
            float* const* outputChannels,
            int outputChannelCount,
            int frameCount);

    static constexpr float outputHeadroom = 0.125f;
    static constexpr size_t maximumEventsPerChannel = 128;
    static constexpr size_t maximumScheduledEvents = RealtimeMidiEventQueue::capacity * 2;

    PreparedGraph* preparedGraph {};
    std::array<Voice, voiceCount> voices;
    MidiControlState midiControls;
    std::array<RealtimeMidiEvent, maximumScheduledEvents> scheduledEvents;
    size_t scheduledEventCount {};
    std::array<float, 8192> metricsScratch;
    uint64_t nextVoiceOrder {};

    std::atomic<uint64_t> callbackCounter {};
    std::atomic<uint64_t> activeRevision {};
    std::atomic<size_t> activeVoices {};
    std::atomic<float> outputPeak {};
    std::atomic<float> outputRms {};
};

}
