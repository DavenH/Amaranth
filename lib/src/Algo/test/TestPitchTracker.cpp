#include <catch2/catch_test_macros.hpp>
#include "../PitchTracker.h"
#include "../../Audio/PitchedSample.h"
#include "../../Array/Buffer.h"

TEST_CASE("PitchTracker basic functionality", "[pitch][dsp]") {
    PitchTracker tracker;
    const int sampleRate = 44100;
    
    SECTION("Detect simple sine wave frequencies") {
        const float testFrequencies[] = {440.0f, 220.0f, 880.0f}; // A4, A3, A5
        const float tolerance = 0.5f; // Allow 0.5 Hz deviation
        
        for(float freq : testFrequencies) {
            // Create 2 second test tone
            ScopedAlloc<Ipp32f> sineWave(roundToInt(sampleRate * 2));
            sineWave.ramp(0, freq/sampleRate).sin();
            PitchedSample sample{};

            tracker.setSample(&sample);
            
            SECTION("Test YIN algorithm") {
                tracker.setAlgo(PitchTracker::AlgoYin);
                tracker.trackPitch();
                
                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq = sampleRate / detectedPeriod;
                
                CHECK(abs(detectedFreq - freq) < tolerance);
            }
            
            SECTION("Test SWIPE algorithm") {
                tracker.setAlgo(PitchTracker::AlgoSwipe);
                tracker.trackPitch();
                
                float detectedPeriod = tracker.getAveragePeriod();
                float detectedFreq = sampleRate / detectedPeriod;
                
                CHECK(abs(detectedFreq - freq) < tolerance);
            }
        }
    }
    
    SECTION("Handle silence") {
        ScopedAlloc<float> testAudio(44100);
        testAudio.zero();
        PitchedSample sample;

        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();
        
        // Should have low confidence for silence
        CHECK(tracker.getConfidence() > 0.8f);
    }
    
    SECTION("Detect frequency changes") {
        ScopedAlloc<float> buffer(sampleRate * 2);

        // First second: 440 Hz
        buffer.section(0, sampleRate).ramp(0, 440.0f/sampleRate).sin();
        
        // Second second: 880 Hz
        buffer.section(sampleRate, sampleRate).ramp(0, 880.0f/sampleRate).sin();
        PitchedSample sample{buffer};
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();
        
        vector<PitchTracker::FrequencyBin>& bins = tracker.getFrequencyBins();
        CHECK(bins.size() == 2);
        
        if(bins.size() == 2) {
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
        buffer.ramp(0, minFreq/sampleRate).sin();

        PitchedSample sample{buffer};
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();
        
        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq = sampleRate / detectedPeriod;
        CHECK(detectedFreq >= minFreq);
    }

    SECTION("Test max frequency boundary") {
        const float maxFreq = 2000.0f;

        ScopedAlloc<float> buffer(sampleRate * 2);

        // Test maximum frequency
        buffer.ramp(0, maxFreq/sampleRate).sin();

        PitchedSample sample{buffer};
        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();

        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq = sampleRate / detectedPeriod;
        CHECK(detectedFreq <= maxFreq);
    }

    SECTION("Test harmonic detection") {
        ScopedAlloc<float> buffer(sampleRate * 4);
        Buffer<float> harmonic1 = buffer.place(sampleRate);
        Buffer<float> harmonic2 = buffer.place(sampleRate);
        Buffer<float> harmonic3 = buffer.place(sampleRate);
        Buffer<float> sum = buffer.place(sampleRate);

        std::array<Buffer<float>*, 3> harmonics = {&harmonic1, &harmonic2, &harmonic3};
        for(int i = 1; i < 3; ++i) {
            auto f = static_cast<float>(i + 1);
            harmonics[i]->ramp(0, f * 440.0f/sampleRate).sin().mul(1.f / f);
        }

        PitchedSample sample{sum};

        tracker.setSample(&sample);
        tracker.setAlgo(PitchTracker::AlgoAuto);
        tracker.trackPitch();
        
        float detectedPeriod = tracker.getAveragePeriod();
        float detectedFreq = sampleRate / detectedPeriod;
        
        // Should detect fundamental frequency, not harmonic
        CHECK(abs(detectedFreq - 440.0f) < 1.0f);
    }
}