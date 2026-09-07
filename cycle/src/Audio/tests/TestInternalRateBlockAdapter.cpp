#include <catch2/catch_test_macros.hpp>

#include <Audio/InternalRateBlockAdapter.h>

TEST_CASE("Internal-rate MIDI carry is consumed exactly once", "[cycle][audio][midi]") {
    InternalRateBlockAdapter adapter;
    adapter.prepare(192000.0);

    juce::MidiBuffer source;
    juce::MidiBuffer converted;

    REQUIRE(adapter.convertBlock(1, source, converted) == 1);
    REQUIRE(converted.isEmpty());

    source.addEvent(juce::MidiMessage::noteOn(1, 60, 0.75f), 0);
    REQUIRE(adapter.convertBlock(1, source, converted) == 0);
    REQUIRE(converted.isEmpty());

    source.clear();
    source.addEvent(juce::MidiMessage::controllerEvent(1, 1, 96), 0);
    REQUIRE(adapter.convertBlock(1, source, converted) == 0);
    REQUIRE(converted.isEmpty());

    source.clear();
    REQUIRE(adapter.convertBlock(1, source, converted) == 0);
    REQUIRE(converted.isEmpty());

    REQUIRE(adapter.convertBlock(1, source, converted) == 1);
    REQUIRE(converted.getNumEvents() == 2);

    juce::Array<juce::MidiMessage> messages;
    for (const juce::MidiMessageMetadata metadata : converted) {
        messages.add(metadata.getMessage());
    }

    REQUIRE(messages[0].isNoteOn());
    REQUIRE(messages[0].getNoteNumber() == 60);
    REQUIRE(messages[1].isController());
    REQUIRE(messages[1].getControllerNumber() == 1);

    REQUIRE(adapter.convertBlock(1, source, converted) == 0);
    REQUIRE(converted.isEmpty());

    while (adapter.convertBlock(1, source, converted) == 0) {
        REQUIRE(converted.isEmpty());
    }
    REQUIRE(converted.isEmpty());
}

TEST_CASE("Internal-rate blocks preserve cumulative time and MIDI positions", "[cycle][audio][midi]") {
    InternalRateBlockAdapter adapter;
    adapter.prepare(48000.0);

    constexpr int outputBlockSize = 127;
    constexpr int outputSamples = 48000;
    int convertedSamples = 0;
    juce::MidiBuffer emptyMidi;
    juce::MidiBuffer converted;

    for (int startSample = 0; startSample < outputSamples; startSample += outputBlockSize) {
        int blockSamples = juce::jmin(outputBlockSize, outputSamples - startSample);
        convertedSamples += adapter.convertBlock(blockSamples, emptyMidi, converted);
    }

    REQUIRE(converted.isEmpty());
    REQUIRE(convertedSamples == 44100);

    adapter.prepare(48000.0);
    juce::MidiBuffer positionedMidi;
    positionedMidi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.5f), 10);

    REQUIRE(adapter.convertBlock(outputBlockSize, positionedMidi, converted) == 117);
    REQUIRE(converted.getNumEvents() == 1);
    REQUIRE((*converted.begin()).samplePosition == 9);
}
