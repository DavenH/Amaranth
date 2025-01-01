#include <catch2/catch_test_macros.hpp>
#include "JuceHeader.h"
using namespace juce;
#include "../src/Algo/PitchTracker.h"
#include "../src/Audio/PitchedSample.h"
#include "../src/Array/Buffer.h"

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

TEST_CASE("PitchTracker basic functionality", "[pitch][dsp]") {
    PitchTracker tracker;
    const int sampleRate = 44100;

    SECTION("Detect simple sine wave frequencies") {
        const float testFrequencies[] = { 440.0f }; // A4, A3, A5
        const float tolerance         = 0.5f;       // Allow 0.5 Hz deviation

        for (float freq: testFrequencies) {
            // Create 2 second test tone
            ScopedAlloc<Ipp32f> signal(roundToInt(sampleRate * 2));
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
            ScopedAlloc<Ipp32f> signal(roundToInt(sampleRate * 2));
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
        CHECK(tracker.getConfidence() > 0.8f);
    }

    SECTION("Detect frequency changes") {
        ScopedAlloc<float> buffer(sampleRate * 2);

        // First second: 440 Hz
        buffer.section(0, sampleRate).ramp(0, 440.0f / sampleRate).sin();

        // Second second: 880 Hz
        buffer.section(sampleRate, sampleRate).ramp(0, 880.0f / sampleRate).sin();
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
        buffer.ramp(0, minFreq / sampleRate).sin();

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
        buffer.ramp(0, maxFreq / sampleRate).sin();

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
        for (int i = 1; i < 3; ++i) {
            auto f = static_cast<float>(i + 1);
            harmonics[i]->ramp(0, f * 440.0f / sampleRate).sin().mul(1.f / f);
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
