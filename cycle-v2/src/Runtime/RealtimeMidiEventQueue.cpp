#include "RealtimeMidiEventQueue.h"

namespace CycleV2 {

using namespace juce;

namespace {

uint32_t mixLifecycleSeed(double timestampSeconds, uint64_t sequence) {
    const uint64_t timestampMicros = timestampSeconds > 0.0
            ? (uint64_t) (timestampSeconds * 1000000.0)
            : 0u;
    uint64_t value = timestampMicros ^ (sequence + 0x9e3779b97f4a7c15ull);
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
    value ^= value >> 31u;
    return (uint32_t) value ^ (uint32_t) (value >> 32u);
}

}

RealtimeMidiEventQueue::RealtimeMidiEventQueue() {
    static_assert((capacity & (capacity - 1)) == 0, "MIDI queue capacity must be a power of two");
    for (size_t index = 0; index < slots.size(); ++index) {
        slots[index].sequence.store(index, std::memory_order_relaxed);
    }
}

MidiMessage RealtimeMidiEvent::toMidiMessage() const {
    switch (kind) {
        case Kind::NoteOn:
            return MidiMessage::noteOn((int) channel, (int) data1, data2);

        case Kind::NoteOff:
            return MidiMessage::noteOff((int) channel, (int) data1, data2);

        case Kind::Controller:
            return MidiMessage::controllerEvent((int) channel, (int) data1, (int) data2);

        case Kind::ChannelPressure:
            return MidiMessage::channelPressureChange((int) channel, (int) data1);

        case Kind::AllNotesOff:
            return MidiMessage::allNotesOff((int) channel);
    }

    return {};
}

bool RealtimeMidiEventQueue::enqueue(
        const MidiMessage& message,
        MidiEventSource source,
        double timestampSeconds) {
    RealtimeMidiEvent converted;
    const uint64_t sequence = nextSequence.fetch_add(1, std::memory_order_relaxed);
    if (!convert(message, source, timestampSeconds, sequence, converted)) {
        return false;
    }

    size_t position = enqueuePosition.load(std::memory_order_relaxed);
    for (;;) {
        Slot& slot = slots[position & (capacity - 1)];
        const size_t slotSequence = slot.sequence.load(std::memory_order_acquire);
        const intptr_t difference = (intptr_t) slotSequence - (intptr_t) position;
        if (difference == 0) {
            if (enqueuePosition.compare_exchange_weak(
                    position,
                    position + 1,
                    std::memory_order_relaxed)) {
                slot.event = converted;
                slot.sequence.store(position + 1, std::memory_order_release);
                return true;
            }
        } else if (difference < 0) {
            droppedEvents.fetch_add(1, std::memory_order_relaxed);
            recoveryRequested[sourceIndex(source)].store(true, std::memory_order_release);
            return false;
        } else {
            position = enqueuePosition.load(std::memory_order_relaxed);
        }
    }
}

bool RealtimeMidiEventQueue::dequeue(RealtimeMidiEvent& event) {
    size_t position = dequeuePosition.load(std::memory_order_relaxed);
    Slot& slot = slots[position & (capacity - 1)];
    const size_t slotSequence = slot.sequence.load(std::memory_order_acquire);
    const intptr_t difference = (intptr_t) slotSequence - (intptr_t) (position + 1);
    if (difference != 0) {
        return false;
    }

    dequeuePosition.store(position + 1, std::memory_order_relaxed);
    event = slot.event;
    slot.sequence.store(position + capacity, std::memory_order_release);
    return true;
}

bool RealtimeMidiEventQueue::consumeRecoveryRequest(MidiEventSource source) {
    return recoveryRequested[sourceIndex(source)].exchange(false, std::memory_order_acq_rel);
}

bool RealtimeMidiEventQueue::convert(
        const MidiMessage& message,
        MidiEventSource source,
        double timestampSeconds,
        uint64_t sequence,
        RealtimeMidiEvent& event) {
    event.source = source;
    event.channel = (uint8_t) jlimit(1, 16, message.getChannel());
    event.timestampSeconds = timestampSeconds;
    event.sequence = sequence;
    event.lifecycleSeed = mixLifecycleSeed(timestampSeconds, sequence);

    if (message.isNoteOn()) {
        event.kind = RealtimeMidiEvent::Kind::NoteOn;
        event.data1 = (uint8_t) message.getNoteNumber();
        event.data2 = (uint8_t) roundToInt(message.getFloatVelocity() * 127.f);
        return true;
    }
    if (message.isNoteOff()) {
        event.kind = RealtimeMidiEvent::Kind::NoteOff;
        event.data1 = (uint8_t) message.getNoteNumber();
        event.data2 = (uint8_t) roundToInt(message.getFloatVelocity() * 127.f);
        return true;
    }
    if (message.isController()) {
        event.kind = RealtimeMidiEvent::Kind::Controller;
        event.data1 = (uint8_t) message.getControllerNumber();
        event.data2 = (uint8_t) message.getControllerValue();
        return true;
    }
    if (message.isChannelPressure()) {
        event.kind = RealtimeMidiEvent::Kind::ChannelPressure;
        event.data1 = (uint8_t) message.getChannelPressureValue();
        return true;
    }
    if (message.isAllNotesOff() || message.isAllSoundOff()) {
        event.kind = RealtimeMidiEvent::Kind::AllNotesOff;
        return true;
    }

    return false;
}

size_t RealtimeMidiEventQueue::sourceIndex(MidiEventSource source) {
    return source == MidiEventSource::PerformanceKeyboard ? 0u : 1u;
}

}
