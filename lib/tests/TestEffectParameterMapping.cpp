#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include <Audio/CycleDsp/EffectParameterMapping.h>

using Catch::Approx;

TEST_CASE("Effect parameter mappings preserve Cycle controls", "[CycleDsp][effects][mapping]") {
    REQUIRE(CycleDsp::equalizerGainDecibels(0.f) == Approx(-30.f));
    REQUIRE(CycleDsp::equalizerGainDecibels(0.5f) == Approx(0.f));
    REQUIRE(CycleDsp::equalizerGainDecibels(1.f) == Approx(30.f));
    REQUIRE(CycleDsp::equalizerGainSnappedUnitValue(0.51f, 500.f) == Approx(0.5f));
    REQUIRE(CycleDsp::equalizerGainSnappedUnitValue(0.52f, 500.f) == Approx(0.52f));

    for (float frequency : { 60.f, 250.f, 1200.f, 4000.f, 8000.f }) {
        REQUIRE(CycleDsp::equalizerFrequency(
                CycleDsp::equalizerFrequencyUnitValue(frequency)) == Approx(frequency));
    }

    REQUIRE(CycleDsp::reverbKernelLength(0.f) == 4096);
    REQUIRE(CycleDsp::reverbKernelLength(1.f) == 262144);
    for (int step = 0; step < CycleDsp::reverbSizeStepCount; ++step) {
        const float unitValue = CycleDsp::reverbSizeUnitValueForStep(step);
        REQUIRE(CycleDsp::reverbKernelLength(unitValue) == (size_t) (4096 << step));
        REQUIRE(CycleDsp::reverbKernelSeconds(unitValue, 44100.0)
                == Approx((double) (4096 << step) / 44100.0));
    }
    REQUIRE(CycleDsp::reverbKernelSeconds(0.5f, 44100.0) == Approx(32768.0 / 44100.0));
    REQUIRE(CycleDsp::reverbDamping(1.f) == Approx(0.7f));
    REQUIRE(CycleDsp::reverbWetLevel(1.f) == Approx(0.25f));

    REQUIRE(CycleDsp::delayBeats(0.f, 4) == Approx(0.09));
    REQUIRE(CycleDsp::delayBeats(0.5f, 4) == Approx(1.0));
    REQUIRE(CycleDsp::delayBeats(1.f, 4) == Approx(4.0));
    REQUIRE(CycleDsp::delayUnitValueForBeats(1.0, 4) == Approx(0.5));
    REQUIRE(CycleDsp::delayUnitValueForBeats(2.0, 4) == Approx(std::sqrt(0.5)));
    REQUIRE(CycleDsp::delayUnitValueForBeats(4.0, 4) == Approx(1.0));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.519f, 4, 500.f) == Approx(0.5));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.521f, 4, 500.f) == Approx(0.521f));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.20f, 4, 500.f) == Approx(0.20f));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.36f, 4, 500.f)
            == Approx(std::sqrt(0.125)));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.38f, 4, 500.f) == Approx(0.38f));
    REQUIRE(CycleDsp::delaySnappedUnitValue(0.695f, 4, 500.f)
            == Approx(std::sqrt(0.5)));
}
