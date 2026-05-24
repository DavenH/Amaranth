#include "../src/RealTimePitchTracker.h"
#include "../../lib/src/Audio/PitchedSample.h"
#include "../../lib/tests/TestDefs.h"
#include "../../lib/tests/SignalDebugger.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

namespace {
    struct RealtimeAlgorithmCase {
        const char* name;
        RealTimePitchTracker::Algorithm algorithm;
        int toleranceSemitones;
    };

    void streamToTracker(RealTimePitchTracker& tracker, Buffer<float> signal, int chunkSize) {
        int offset = 0;
        while (offset < signal.size()) {
            const int size = jmin(chunkSize, signal.size() - offset);
            Buffer<float> chunk = signal.section(offset, size);
            tracker.write(chunk);
            offset += size;
        }
    }

    void addHarmonicTone(Buffer<float> signal, float frequency, float sampleRate) {
        signal.zero();
        ScopedAlloc<float> harmonicWave(signal.size());
        for (int harmonic = 1; harmonic <= 6; ++harmonic) {
            harmonicWave.zero();
            harmonicWave.sin((frequency * static_cast<float>(harmonic)) / sampleRate)
                .mul(1.0f / (float) harmonic);
            signal.add(harmonicWave);
        }
        signal.mul(1.0f / jmax(0.01f, signal.max()));
    }

    std::vector<float> loadMonoAudioFile(const File& file, double& sampleRate, int maxSamples) {
        AudioFormatManager formats;
        formats.registerBasicFormats();

        std::unique_ptr<AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr) {
            return {};
        }

        sampleRate = reader->sampleRate;
        const int numSamples = jmin(maxSamples, (int) reader->lengthInSamples);
        AudioBuffer<float> audio((int) reader->numChannels, numSamples);
        reader->read(&audio, 0, numSamples, 0, true, true);

        std::vector<float> mono((size_t) numSamples);
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            float value = 0.f;
            for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
                value += audio.getSample(channel, i);
            }
            mono[(size_t) i] = value / (float) audio.getNumChannels();
        }

        return mono;
    }

    File findRepoFile(const String& relativePath) {
        File dir = File::getCurrentWorkingDirectory();
        for (int i = 0; i < 8; ++i) {
            File candidate = dir.getChildFile(relativePath);
            if (candidate.existsAsFile()) {
                return candidate;
            }
            dir = dir.getParentDirectory();
        }
        return {};
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
    constexpr int midiNote = 69; // A4
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

TEST_CASE("RealTimePitchTracker exposes selectable realtime algorithms for steady tones", "[pitch][realtime][oscillo]") {
    constexpr float sampleRate = 44100.0f;
    constexpr int midiNote = 57; // A3
    const float frequency = MidiMessage::getMidiNoteInHertz(midiNote);

    const RealtimeAlgorithmCase algorithms[] = {
        { "spectral",   RealTimePitchTracker::AlgoSpectral,  0 },
        { "cycle-diff", RealTimePitchTracker::AlgoCycleDiff, 12 },
        { "yin",        RealTimePitchTracker::AlgoYin,       1 },
        { "swipe",      RealTimePitchTracker::AlgoSwipe,     1 },
    };

    for (const auto& algorithm: algorithms) {
        DYNAMIC_SECTION(algorithm.name) {
            RealTimePitchTracker tracker(algorithm.algorithm);
            tracker.setSampleRate((int) sampleRate);

            ScopedAlloc<float> signal(4096 * 4);
            signal.sin(frequency / sampleRate);
            streamToTracker(tracker, signal, 257);

            const auto result = tracker.update();
            CHECK(std::abs(result - midiNote) <= algorithm.toleranceSemitones);
        }
    }
}

TEST_CASE("RealTimePitchTracker smoke-tests tuning sample when available", "[pitch][realtime][oscillo][sample]") {
    const File sampleFile = findRepoFile("oscillo/content/tuning-sample.mp3");

    if (!sampleFile.existsAsFile()) {
        WARN("oscillo/content/tuning-sample.mp3 not present; skipping optional tuning-sample smoke test");
        return;
    }

    double sampleRate = 0.0;
    const std::vector<float> mono = loadMonoAudioFile(sampleFile, sampleRate, 44100 * 20);
    if (mono.empty()) {
        WARN("Could not decode tuning-sample.mp3; skipping optional tuning-sample smoke test");
        return;
    }

    RealTimePitchTracker tracker(RealTimePitchTracker::AlgoYin);
    tracker.setSampleRate((int) sampleRate);

    constexpr int stride = 4096;
    ScopedAlloc<float> chunk(stride);
    std::vector<int> detectedNotes;

    for (int offset = 0; offset + stride <= (int) mono.size(); offset += stride) {
        chunk.zero();
        Buffer<float> chunkBuffer(chunk);
        for (int i = 0; i < stride; ++i) {
            chunkBuffer[i] = mono[(size_t) (offset + i)];
        }

        if (chunkBuffer.normL2() < 1.0f) {
            continue;
        }

        tracker.write(chunkBuffer);
        const int detected = tracker.update();
        if (detectedNotes.empty() || detected != detectedNotes.back()) {
            detectedNotes.push_back(detected);
        }
    }

    REQUIRE(!detectedNotes.empty());

    for (int detected: detectedNotes) {
        CAPTURE(detected);
        CHECK(detected >= 21);
        CHECK(detected <= 108);
    }

    if (detectedNotes.size() < 2) {
        WARN("tuning-sample.mp3 produced fewer than two note regions without metadata-guided onset windows");
    }
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
