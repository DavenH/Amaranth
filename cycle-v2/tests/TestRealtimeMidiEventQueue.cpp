#include <catch2/catch_test_macros.hpp>

#include "Runtime/RealtimeMidiEventQueue.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Realtime MIDI queue preserves supported event data and source",
        "[cycle-v2][midi][realtime]") {
    RealtimeMidiEventQueue queue;
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(2, 64, (uint8) 101),
            MidiEventSource::PerformanceKeyboard,
            12.5));
    REQUIRE(queue.enqueue(
            MidiMessage::controllerEvent(2, 1, 93),
            MidiEventSource::Hardware,
            12.6));

    RealtimeMidiEvent event;
    REQUIRE(queue.dequeue(event));
    REQUIRE(event.kind == RealtimeMidiEvent::Kind::NoteOn);
    REQUIRE(event.source == MidiEventSource::PerformanceKeyboard);
    REQUIRE(event.channel == 2);
    REQUIRE(event.data1 == 64);
    REQUIRE(event.data2 == 101);
    REQUIRE(event.timestampSeconds == 12.5);
    const uint32_t noteLifecycleSeed = event.lifecycleSeed;

    REQUIRE(queue.dequeue(event));
    REQUIRE(event.kind == RealtimeMidiEvent::Kind::Controller);
    REQUIRE(event.source == MidiEventSource::Hardware);
    REQUIRE(event.data1 == 1);
    REQUIRE(event.data2 == 93);
    REQUIRE(event.lifecycleSeed != noteLifecycleSeed);
    REQUIRE_FALSE(queue.dequeue(event));
}

TEST_CASE("Realtime MIDI retriggers receive distinct clock-scoped lifecycle seeds",
        "[cycle-v2][midi][realtime][guide]") {
    RealtimeMidiEventQueue queue;
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::Hardware,
            42.25));
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::Hardware,
            42.25));

    RealtimeMidiEvent first;
    RealtimeMidiEvent retrigger;
    REQUIRE(queue.dequeue(first));
    REQUIRE(queue.dequeue(retrigger));
    REQUIRE(first.lifecycleSeed != retrigger.lifecycleSeed);

    RealtimeMidiEventQueue laterQueue;
    REQUIRE(laterQueue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::Hardware,
            42.5));
    RealtimeMidiEvent later;
    REQUIRE(laterQueue.dequeue(later));
    REQUIRE(later.lifecycleSeed != first.lifecycleSeed);
}

TEST_CASE("Realtime MIDI queue overflow requests source-scoped recovery",
        "[cycle-v2][midi][realtime]") {
    RealtimeMidiEventQueue queue;
    for (size_t index = 0; index < RealtimeMidiEventQueue::capacity; ++index) {
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 60, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                (double) index));
    }

    REQUIRE_FALSE(queue.enqueue(
            MidiMessage::noteOff(1, 60),
            MidiEventSource::PerformanceKeyboard,
            1000.0));
    REQUIRE(queue.droppedEventCount() == 1);
    REQUIRE(queue.consumeRecoveryRequest(MidiEventSource::PerformanceKeyboard));
    REQUIRE_FALSE(queue.consumeRecoveryRequest(MidiEventSource::PerformanceKeyboard));
}
