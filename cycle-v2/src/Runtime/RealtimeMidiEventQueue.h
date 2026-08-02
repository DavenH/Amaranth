#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace CycleV2 {

enum class MidiEventSource : uint8_t {
    PerformanceKeyboard,
    Hardware
};

struct RealtimeMidiEvent {
    enum class Kind : uint8_t {
        NoteOn,
        NoteOff,
        Controller,
        ChannelPressure,
        AllNotesOff
    };

    Kind kind { Kind::NoteOn };
    MidiEventSource source { MidiEventSource::PerformanceKeyboard };
    uint8_t channel { 1 };
    uint8_t data1 {};
    uint8_t data2 {};
    double timestampSeconds {};
    uint64_t sequence {};

    juce::MidiMessage toMidiMessage() const;
};

class MidiEventSink {
public:
    virtual ~MidiEventSink() = default;

    virtual bool enqueueMidiMessage(
            const juce::MidiMessage& message,
            MidiEventSource source) = 0;
    virtual void releaseMidiSource(MidiEventSource source) = 0;
};

class RealtimeMidiEventQueue {
public:
    static constexpr size_t capacity = 512;

    RealtimeMidiEventQueue();

    bool enqueue(
            const juce::MidiMessage& message,
            MidiEventSource source,
            double timestampSeconds);
    bool dequeue(RealtimeMidiEvent& event);
    bool consumeRecoveryRequest(MidiEventSource source);
    size_t droppedEventCount() const { return droppedEvents.load(); }

private:
    struct Slot {
        std::atomic<size_t> sequence {};
        RealtimeMidiEvent event;
    };

    static bool convert(
            const juce::MidiMessage& message,
            MidiEventSource source,
            double timestampSeconds,
            uint64_t sequence,
            RealtimeMidiEvent& event);
    static size_t sourceIndex(MidiEventSource source);

    std::array<Slot, capacity> slots;
    std::array<std::atomic<bool>, 2> recoveryRequested {};
    std::atomic<size_t> enqueuePosition {};
    std::atomic<size_t> dequeuePosition {};
    std::atomic<size_t> droppedEvents {};
    std::atomic<uint64_t> nextSequence {};
};

}
