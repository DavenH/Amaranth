#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/EqualizerCore.h>

TEST_CASE("Equalizer core exposes its configured response", "[CycleDsp][equalizer]") {
    CycleDsp::EqualizerCore equalizer(2);
    for (int band = 0; band < CycleDsp::EqualizerCore::bandCount; ++band) {
        equalizer.configureBand(band, 48000.0, 1000.f, band == 2 ? 18.f : 0.f);
    }

    REQUIRE(equalizer.responseDecibels(1000.0) > 10.f);
    REQUIRE(equalizer.responseDecibels(100.0) < equalizer.responseDecibels(1000.0));
}

TEST_CASE("Equalizer core keeps channel histories independent", "[CycleDsp][equalizer]") {
    CycleDsp::EqualizerCore equalizer(2);
    for (int band = 0; band < CycleDsp::EqualizerCore::bandCount; ++band) {
        equalizer.configureBand(band, 48000.0, 1000.f, band == 2 ? 18.f : 0.f);
    }

    float leftSamples[] { 1.f, 0.f, 0.f, 0.f };
    float rightSamples[] { 0.f, 0.f, 0.f, 0.f };
    equalizer.process(0, { leftSamples, 4 });
    equalizer.process(1, { rightSamples, 4 });

    REQUIRE(leftSamples[0] != 0.f);
    REQUIRE(rightSamples[0] == 0.f);
}
