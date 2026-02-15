#include "../src/RealTimePitchTracker.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/OscAudioProcessor.h"

class TestableOscAudioProcessor : public OscAudioProcessor {
public:
    explicit TestableOscAudioProcessor(RealTimePitchTracker* pt)
        : OscAudioProcessor(pt) {}

    using OscAudioProcessor::appendSamplesRetractingPeriods;
    using OscAudioProcessor::accumulatedSamples;
    using OscAudioProcessor::targetPeriod;

    // Expose internal state for testing
    float getAccumulatedSamples() const { return accumulatedSamples; }
    void setAccumulatedSamples(float value) { accumulatedSamples = value; }
    void setTargetPeriod(float value) { targetPeriod = value; }
};

class StubPitchTracker : public RealTimePitchTracker {
public:
    void setSampleRate(int) { /* no-op */ }
    void write(Buffer<float>&) { /* no-op */ }
    // update() not used by OscAudioProcessor in these tests
};


TEST_CASE("OscAudioProcessor Period Extraction", "[audio]") {
    StubPitchTracker tracker;
    TestableOscAudioProcessor processor(&tracker);

    SECTION("Simple integer period") {
        float period = 100.f;
        float relFreq = 1 / period;
        processor.setTargetPeriod(period);
        processor.setAccumulatedSamples(0.0f);

        // Create a buffer of 512 samples
        ScopedAlloc<float> audioBlock(512);
        audioBlock.sin(relFreq);
        processor.appendSamplesRetractingPeriods(audioBlock);

        // Should extract exactly 5 complete periods (512/100 = 5 remainder 12)
        auto periods = processor.getAudioPeriods();
        REQUIRE(periods.size() == 5);

        // Check accumulated samples is correct (should be 12.0f)
        float expectedAccumulated = 512.0f - (5.0f * 100.0f);
        REQUIRE(processor.getAccumulatedSamples() == Catch::Approx(expectedAccumulated));
    }

    SECTION("Fractional period (A4 = 44100/440)") {
        float period = 44100.0f / 440.0f; // ≈ 100.227272...
        float relFreq = 1.f / period;
        processor.setTargetPeriod(period);
        processor.setAccumulatedSamples(0.0f);

        // Create a buffer of 512 samples
        ScopedAlloc<float> audioBlock(512);
        audioBlock.sin(relFreq);

        processor.appendSamplesRetractingPeriods(audioBlock);

        // Should extract approximately 5 periods (512/100.227272 ≈ 5.1)
        auto periods = processor.getAudioPeriods();
        REQUIRE(periods.size() == 5);

        // Check accumulated samples is reasonable
        float expectedAccumulated = 512.0f - (5.0f * period);
        REQUIRE(processor.getAccumulatedSamples() == Catch::Approx(expectedAccumulated));
        REQUIRE(processor.getAccumulatedSamples() >= 0.0f);
    }

    SECTION("Multiple calls with fractional period") {
        float relFreq = 440.0f / 44100.0f;
        float period = 1 / relFreq;
        processor.setTargetPeriod(period);
        processor.setAccumulatedSamples(0.0f);

        // Simulate multiple audio callbacks
        for (int call = 0; call < 10; call++) {
            ScopedAlloc<float> audioBlock(512);
            audioBlock.sin(relFreq);

            processor.appendSamplesRetractingPeriods(audioBlock);

            // Check accumulated samples stays bounded
            float accumulated = processor.getAccumulatedSamples();
            REQUIRE(accumulated >= 0.0f);
            REQUIRE(accumulated < 2.0f * period);

            processor.resetPeriods(); // Clear periods between calls
        }
    }

    SECTION("Very small period") {
        float period = 2.5f;
        float relFreq = 1 / period;

        // Test with period = 2.5 samples to verify handling of small periods
        processor.setTargetPeriod(period);
        processor.setAccumulatedSamples(0.0f);

        ScopedAlloc<float> audioBlock(10);
        audioBlock.sin(relFreq);

        processor.appendSamplesRetractingPeriods(audioBlock);

        auto periods = processor.getAudioPeriods();
        // Should alternate between 2 and 3 samples per period
        for (const auto& period : periods) {
            REQUIRE((period.size() == 2 || period.size() == 3));
        }

        float accumulated = processor.getAccumulatedSamples();
        REQUIRE(accumulated >= 0.0f);
        REQUIRE(accumulated <= 2.5f);
    }
}
