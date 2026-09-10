#include <catch2/catch_test_macros.hpp>

#include <Array/ScopedAlloc.h>
#include <Audio/CycleDsp/VoiceDeclick.h>

TEST_CASE("Voice declick preserves legacy timing at each render rate",
        "[audio][declick][parity]") {
    REQUIRE(CycleDsp::VoiceDeclick::attackSampleCount(44'100.) == 64);
    REQUIRE(CycleDsp::VoiceDeclick::releaseSampleCount(44'100.) == 441);
    REQUIRE(CycleDsp::VoiceDeclick::attackSampleCount(48'000.) == 70);
    REQUIRE(CycleDsp::VoiceDeclick::releaseSampleCount(48'000.) == 480);
}

TEST_CASE("Voice declick aligns its release ramp with the envelope tail",
        "[audio][declick][parity]") {
    ScopedAlloc<float> release(CycleDsp::VoiceDeclick::releaseSampleCount(44'100.));
    CycleDsp::VoiceDeclick::prepareRelease(release);

    ScopedAlloc<float> envelope(64);
    envelope.set(1.f);
    CycleDsp::VoiceDeclick::applyReleaseTail(envelope, release, 32);

    REQUIRE(envelope[0] == release[release.size() - 32]);
    REQUIRE(envelope[31] == release.back());
    REQUIRE(envelope[32] == 1.f);
}
