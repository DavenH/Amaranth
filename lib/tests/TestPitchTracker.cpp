#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "JuceHeader.h"
using namespace juce;
#include "../src/Algo/Pitch/PitchTracker.h"
#include "../src/App/Doc/PresetJson.h"
#include "../src/Audio/PitchedSample.h"
#include "../src/Array/Buffer.h"
#include "../src/Util/NumberUtils.h"

namespace {

File getRepositoryRoot() {
    return File(LIB_ROOT).getParentDirectory();
}

File getReferenceWaveFile(const String& fileName) {
    return getRepositoryRoot().getChildFile("cycle/content/wav").getChildFile(fileName);
}

struct ReferencePitchPoint {
    float position = 0.f;
    float unitPitch = 0.5f;
};

struct ReferencePitchEnvelope {
    int midiNote = -1;
    vector<ReferencePitchPoint> points;
};

struct PitchEnvelopeError {
    float normalizedMeanSquaredError = 0.f;
    float rmsSemitones = 0.f;
    float meanSquaredCents = 0.f;
    float rmsCents = 0.f;
    float maxAbsCents = 0.f;
    int numFrames = 0;
};

ReferencePitchEnvelope readReferencePitchEnvelope(const File& waveFile) {
    File envFile = waveFile.getSiblingFile(waveFile.getFileNameWithoutExtension() + ".env");
    REQUIRE(envFile.existsAsFile());

    var envelopeValue = JSON::parse(envFile.loadFileAsString());
    REQUIRE(envelopeValue.isObject());

    ReferencePitchEnvelope envelope;
    envelope.midiNote = PresetJson::intProperty(envelopeValue, "midiNote", -1);
    REQUIRE(envelope.midiNote >= 0);

    var meshValue = PresetJson::property(envelopeValue, "mesh");
    var mainMeshValue = PresetJson::property(meshValue, "mainMesh");
    var verticesValue = PresetJson::property(mainMeshValue, "vertices");
    const Array<var>* vertices = PresetJson::getArray(verticesValue);
    REQUIRE(vertices != nullptr);

    for (const auto& vertexValue: *vertices) {
        if (! vertexValue.isObject()) {
            continue;
        }

        envelope.points.push_back({
            static_cast<float>(PresetJson::doubleProperty(vertexValue, "phase")),
            static_cast<float>(PresetJson::doubleProperty(vertexValue, "amp", 0.5))
        });
    }

    std::sort(envelope.points.begin(), envelope.points.end(), [](
            const ReferencePitchPoint& left,
            const ReferencePitchPoint& right) {
        return left.position < right.position;
    });

    REQUIRE(envelope.points.size() >= 2);
    return envelope;
}

float interpolateUnitPitch(const ReferencePitchEnvelope& envelope, float position) {
    if (position <= envelope.points.front().position) {
        return envelope.points.front().unitPitch;
    }

    if (position >= envelope.points.back().position) {
        return envelope.points.back().unitPitch;
    }

    for (int i = 1; i < (int) envelope.points.size(); ++i) {
        const ReferencePitchPoint& right = envelope.points[i];

        if (position <= right.position) {
            const ReferencePitchPoint& left = envelope.points[i - 1];
            const float width = right.position - left.position;

            if (width <= 0.000001f) {
                return right.unitPitch;
            }

            const float alpha = (position - left.position) / width;
            return left.unitPitch + alpha * (right.unitPitch - left.unitPitch);
        }
    }

    return envelope.points.back().unitPitch;
}

float expectedNoteAt(const ReferencePitchEnvelope& envelope, float position) {
    return envelope.midiNote + static_cast<float>(NumberUtils::unitPitchToSemis(interpolateUnitPitch(envelope, position)));
}

PitchEnvelopeError measurePitchEnvelopeError(PitchTracker& tracker, const File& waveFile, int algorithm) {
    const ReferencePitchEnvelope envelope = readReferencePitchEnvelope(waveFile);

    PitchedSample sample;
    REQUIRE(sample.load(waveFile.getFullPathName()) == 0);
    REQUIRE(sample.size() > 0);

    tracker.setSample(&sample);
    tracker.setAlgo(algorithm);
    tracker.trackPitch();
    REQUIRE(! sample.periods.empty());

    double squaredSemitoneError = 0.;
    double maxAbsSemitoneError = 0.;
    int comparedFrames = 0;

    for (const auto& frame: sample.periods) {
        if (frame.period <= 0.f || frame.sampleOffset < 0 || frame.sampleOffset >= sample.size()) {
            continue;
        }

        const float position = static_cast<float>(frame.sampleOffset) / static_cast<float>(sample.size());

        if (position < envelope.points.front().position || position > envelope.points.back().position) {
            continue;
        }

        const float expectedNote = expectedNoteAt(envelope, position);
        const float detectedNote = static_cast<float>(NumberUtils::frequencyToNote(sample.samplerate / frame.period));
        const double errorSemis = detectedNote - expectedNote;
        squaredSemitoneError += errorSemis * errorSemis;
        maxAbsSemitoneError = jmax(maxAbsSemitoneError, std::abs(errorSemis));
        ++comparedFrames;
    }

    PitchEnvelopeError error;
    error.numFrames = comparedFrames;

    if (comparedFrames > 0) {
        const double meanSquaredSemitoneError = squaredSemitoneError / comparedFrames;
        error.rmsSemitones = static_cast<float>(std::sqrt(meanSquaredSemitoneError));
        error.normalizedMeanSquaredError = static_cast<float>(meanSquaredSemitoneError / (12. * 12.));
        error.meanSquaredCents = static_cast<float>(meanSquaredSemitoneError * 100. * 100.);
        error.rmsCents = error.rmsSemitones * 100.f;
        error.maxAbsCents = static_cast<float>(maxAbsSemitoneError * 100.);
    }

    return error;
}

}

void createSineWave(Buffer<float>& buff, float freq, float sampleRate) {
    buff.ramp(0, 2 * M_PI * freq / sampleRate).sin();
}

void createSawtooth(Buffer<float>& buffer, float frequency, float sampleRate, float phase = 0.0f) {
    const int size = buffer.size();
    if (size == 0) {
        return;
    }

    float samplesPerCycle = sampleRate / frequency;

    buffer.ramp(phase, 1.0f/samplesPerCycle);

    for (int i = 0; i < size; i++) {
        buffer[i] = buffer[i] - std::floor(buffer[i]);
    }

    buffer.mul(2.0f).sub(1.0f);
}

void createExpSweep(Buffer<float>& buffer, float startFreq, float endFreq, float sampleRate) {
    buffer.ramp()                           // 0 to 1 linear ramp
          .mul(std::log(endFreq/startFreq)) // Scale for desired frequency range
          .exp()                            // Make frequency increase exponentially
          .mul(startFreq * 2 * M_PI)        // Scale to start at startFreq
          .sin();                           // Convert phase to sine wave
}

TEST_CASE("PitchTracker basic functionality", "[pitch][dsp][wip]") {
    PitchTracker tracker;
    const int sampleRate = 44100;

    SECTION("Detect simple sine wave frequencies") {
        const float testFrequencies[] = { 440.0f }; // A4, A3, A5
        const float tolerance         = 0.5f;       // Allow 0.5 Hz deviation

        for (float freq: testFrequencies) {
            // Create 2 second test tone
            ScopedAlloc<Float32> signal(roundToInt(sampleRate * 2));
            createSineWave(signal, freq, sampleRate);

            PitchedSample sample(signal);
            sample.samplerate = sampleRate;
            tracker.setSample(&sample);

            SECTION("Test YIN algorithm") {
                tracker.setAlgo(PitchTracker::AlgoYin);
                tracker.trackPitch();

                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq   = sampleRate / detectedPeriod;

                CHECK(abs(detectedFreq - freq) < tolerance);
            }

            SECTION("Test SWIPE algorithm") {
                tracker.setAlgo(PitchTracker::AlgoSwipe);
                tracker.trackPitch();

                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq   = sampleRate / detectedPeriod;

                CHECK(abs(detectedFreq - freq) < tolerance);
            }
        }
    }

    SECTION("Detect simple sawtooth frequencies") {
        const float testFrequencies[] = { 225.0f }; // A4, A3, A5
        const float tolerance         = 0.5f;       // Allow 0.5 Hz deviation

        for (float freq: testFrequencies) {
            // Create 2 second test tone
            ScopedAlloc<Float32> signal(roundToInt(sampleRate * 2));
            createSawtooth(signal, freq, sampleRate);
            // createExpSweep(signal, 40, 800, sampleRate);

            PitchedSample sample(signal);
            sample.samplerate = sampleRate;
            tracker.setSample(&sample);

            SECTION("Test YIN algorithm") {
                tracker.setAlgo(PitchTracker::AlgoYin);
                tracker.trackPitch();

                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq   = sampleRate / detectedPeriod;

                CHECK(abs(detectedFreq - freq) < tolerance);
            }

            SECTION("Test SWIPE algorithm") {
                tracker.setAlgo(PitchTracker::AlgoSwipe);
                tracker.trackPitch();

                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq   = sampleRate / detectedPeriod;

                CHECK(abs(detectedFreq - freq) < tolerance);
            }
        }
    }

    SECTION("Handle silence") {
        ScopedAlloc<float> testAudio(44100);
        testAudio.zero();
        PitchedSample sample(testAudio);

        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        // Should have low confidence for silence
        CHECK(tracker.getConfidence() < 0.8f);
    }

    SECTION("Detect frequency changes") {
        ScopedAlloc<float> buffer(sampleRate * 2);

        // First second: 440 Hz
        Buffer<float> firstHalf = buffer.section(0, sampleRate);
        createSineWave(firstHalf, 440.0f, sampleRate);

        // Second second: 880 Hz
        Buffer<float> secondHalf = buffer.section(sampleRate, sampleRate);
        createSineWave(secondHalf, 880.0f, sampleRate);
        PitchedSample sample(buffer);
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        vector<PitchTracker::FrequencyBin>& bins = tracker.getFrequencyBins();
        CHECK(bins.size() == 2);

        if (bins.size() == 2) {
            float freq1 = sampleRate / bins[0].averagePeriod;
            float freq2 = sampleRate / bins[1].averagePeriod;

            CHECK((abs(freq1 - 440.0f) < 1.0f || abs(freq1 - 880.0f) < 1.0f));
            CHECK((abs(freq2 - 440.0f) < 1.0f || abs(freq2 - 880.0f) < 1.0f));
        }
    }

    SECTION("Test min frequency boundary") {
        const float minFreq = 20.0f;

        ScopedAlloc<float> buffer(sampleRate * 2);

        // Test minimum frequency
        createSineWave(buffer, minFreq, sampleRate);

        PitchedSample sample(buffer);
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq   = sampleRate / detectedPeriod;
        CHECK(detectedFreq >= minFreq);
    }

    SECTION("Test max frequency boundary") {
        const float maxFreq = 2000.0f;

        ScopedAlloc<float> buffer(sampleRate * 2);

        // Test maximum frequency
        createSineWave(buffer, maxFreq, sampleRate);

        PitchedSample sample(buffer);
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq   = sampleRate / detectedPeriod;
        CHECK(detectedFreq <= maxFreq);
    }

    SECTION("Test harmonic detection") {
        ScopedAlloc<float> buffer(sampleRate * 4);
        Buffer<float> harmonic1 = buffer.place(sampleRate);
        Buffer<float> harmonic2 = buffer.place(sampleRate);
        Buffer<float> harmonic3 = buffer.place(sampleRate);
        Buffer<float> sum       = buffer.place(sampleRate);

        std::array<Buffer<float>*, 3> harmonics = { &harmonic1, &harmonic2, &harmonic3 };
        sum.zero();

        for (int i = 0; i < 3; ++i) {
            auto f = static_cast<float>(i + 1);
            harmonics[i]->ramp(0, f * 440.0f / sampleRate).sin().mul(1.f / f);
            sum.add(*harmonics[i]);
        }

        PitchedSample sample(sum);

        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq   = sampleRate / detectedPeriod;

        // Should detect fundamental frequency, not harmonic
        CHECK(abs(detectedFreq - 440.0f) < 1.0f);
    }
}

TEST_CASE("PitchTracker validates reference wavs against curated pitch envelopes", "[pitch][dsp][reference]") {
    struct ReferenceCase {
        const char* fileName;
        float maxRmsCents;
    };

    const ReferenceCase references[] = {
        { "analogue2.wav",       5.f },
        { "bagpipes.wav",        6.f },
        { "bassdeep.wav",        8.5f },
        { "deepforest.wav",      7.f },
        { "deffbass.wav",        6.f },
        { "dist3.wav",           28.f },
        { "dist4.wav",           6.f },
        { "dist5.wav",           0.5f },
        { "dist6.wav",           0.5f },
        { "fatbass.wav",         0.001f },
        { "fxbass.wav",          25.f },
        { "harpsiC3.wav",        0.75f },
        { "maxmog.wav",          0.25f },
        { "mesa1.wav",           0.5f },
        { "noisy-flute.wav",     6.f },
        { "powerchord3_2.wav",   0.25f },
        { "sax-growl.wav",       0.5f },
        { "sitar3.wav",          12.f },
    };

    PitchTracker tracker;

    for (const auto& reference: references) {
        File waveFile = getReferenceWaveFile(reference.fileName);
        REQUIRE(waveFile.existsAsFile());

        const PitchEnvelopeError error = measurePitchEnvelopeError(tracker, waveFile, PitchTracker::AlgoAuto);

        std::cout
            << "PitchTracker reference error sample=" << reference.fileName
            << " frames=" << error.numFrames
            << " rmsCents=" << error.rmsCents
            << " meanSquaredCents=" << error.meanSquaredCents
            << " maxAbsCents=" << error.maxAbsCents
            << " normalizedMse=" << error.normalizedMeanSquaredError
            << std::endl;

        CAPTURE(
            reference.fileName,
            error.numFrames,
            error.rmsCents,
            error.meanSquaredCents,
            error.maxAbsCents,
            error.normalizedMeanSquaredError);
        CHECK(error.numFrames > 0);
        CHECK(error.rmsCents <= reference.maxRmsCents);
    }
}
