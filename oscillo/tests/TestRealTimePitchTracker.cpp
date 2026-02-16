#include "../src/RealTimePitchTracker.h"
#include "../../lib/src/Audio/PitchedSample.h"
#include "../../lib/tests/TestDefs.h"
#include "../../lib/tests/SignalDebugger.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
    void streamToTracker(RealTimePitchTracker& tracker, Buffer<float> signal, int chunkSize) {
        int offset = 0;
        while (offset < signal.size()) {
            const int size = jmin(chunkSize, signal.size() - offset);
            Buffer<float> chunk = signal.section(offset, size);
            tracker.write(chunk);
            offset += size;
        }
    }
}

TEST_CASE("RealTimePitchTracker tracks equal-tempered notes exactly", "[pitch][realtime][oscillo]") {
    RealTimePitchTracker tracker;
    constexpr float sampleRate = 44100.0f;
    tracker.setSampleRate((int) sampleRate);

    const int midiNotes[] = {45, 57, 69, 81};

    for (int midiNote : midiNotes) {
        const float frequency = MidiMessage::getMidiNoteInHertz(midiNote);
        ScopedAlloc<float> signal(4096 * 3);
        signal.sin(frequency / sampleRate);

        streamToTracker(tracker, signal, 256);
        const auto result = tracker.update();

        CHECK(result == midiNote);
    }
}

TEST_CASE("RealTimePitchTracker handles rich harmonic content exactly", "[pitch][realtime][oscillo]") {
    RealTimePitchTracker tracker;
    constexpr float sampleRate = 48000.0f;
    tracker.setSampleRate((int) sampleRate);
    constexpr int midiNote = 57; // A3
    const float frequency = MidiMessage::getMidiNoteInHertz(midiNote);
    ScopedAlloc<float> signal(4096 * 3);
    signal.zero();
    ScopedAlloc<float> harmonicWave(signal.size());
    for (int harmonic = 1; harmonic <= 8; ++harmonic) {
        harmonicWave.zero();
        harmonicWave.sin((frequency * static_cast<float>(harmonic)) / sampleRate).mul(1.0f / (float) harmonic);
        signal.add(harmonicWave);
    }

    streamToTracker(tracker, signal, 192);
    const auto result = tracker.update();

    CHECK(result == midiNote);
}
/*
TEST_CASE("RealTimePitchTracker is robust to moderate noise", "[pitch][realtime][oscillo]") {
    RealTimePitchTracker tracker;
    constexpr float sampleRate = 44100.0f;
    tracker.setSampleRate((int) sampleRate);

    constexpr int midiNote = 64; // E4
    const float frequency = MidiMessage::getMidiNoteInHertz(midiNote);
    ScopedAlloc<float> signal(4096 * 4);
    signal.sin(frequency / sampleRate).mul(0.7f);
    ScopedAlloc<float> noise(signal.size());
    unsigned noiseSeed = 12345;
    noise.rand(noiseSeed).mul(2.0f).sub(1.0f).mul(0.08f);
    signal.add(noise);

    streamToTracker(tracker, signal, 320);
    const auto result = tracker.update();

    CHECK(result == midiNote);
}*/

TEST_CASE("RealTimePitchTracker follows note changes in streaming input", "[pitch][realtime][oscillo]") {
    RealTimePitchTracker tracker;
    constexpr float sampleRate = 44100.0f;
    tracker.setSampleRate((int) sampleRate);

    constexpr int firstNote = 57;  // A3
    constexpr int secondNote = 69; // A4
    const float firstFrequency = MidiMessage::getMidiNoteInHertz(firstNote);
    const float secondFrequency = MidiMessage::getMidiNoteInHertz(secondNote);

    ScopedAlloc<float> firstSignal(4096 * 2);
    firstSignal.sin(firstFrequency / sampleRate);
    streamToTracker(tracker, firstSignal, 256);

    const auto firstResult = tracker.update();
    CHECK(firstResult == firstNote);

    ScopedAlloc<float> secondSignal(4096 * 2);
    secondSignal.sin(secondFrequency / sampleRate);
    streamToTracker(tracker, secondSignal, 256);

    const auto secondResult = tracker.update();
    CHECK(secondResult == secondNote);
}
/*
TEST_CASE("RealTimePitchTracker keeps last valid pitch through silence", "[pitch][realtime][oscillo]") {
    RealTimePitchTracker tracker;
    constexpr float sampleRate = 44100.0f;
    tracker.setSampleRate((int) sampleRate);

    constexpr int midiNote = 69; // A4
    const float frequency = MidiMessage::getMidiNoteInHertz(midiNote);
    ScopedAlloc<float> voiced(4096 * 2);
    voiced.sin(frequency / sampleRate);
    streamToTracker(tracker, voiced, 256);
    const auto voicedResult = tracker.update();
    REQUIRE(voicedResult == midiNote);

    ScopedAlloc<float> silence(4096 * 2);
    silence.zero();
    streamToTracker(tracker, silence, 256);
    const auto silentResult = tracker.update();

    CHECK(silentResult == voicedResult);
}
*/