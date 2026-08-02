#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Audio/CycleDsp/OscillatorLaneCore.h>

using Catch::Matchers::WithinAbs;

TEST_CASE("Oscillator lane pitch preserves the Cycle 1 angle-delta contract",
        "[cycle-dsp][oscillator-lane]") {
    const double middleC = CycleDsp::OscillatorLaneCore::angleDelta(60, 0.f, 44100.0);
    const double octave = CycleDsp::OscillatorLaneCore::angleDelta(72, 0.f, 44100.0);
    const double detuned = CycleDsp::OscillatorLaneCore::angleDelta(60, 1200.f, 44100.0);

    REQUIRE_THAT(octave, WithinAbs(middleC * 2.0, 1.0e-12));
    REQUIRE_THAT(detuned, WithinAbs(middleC * 2.0, 1.0e-12));
    REQUIRE(CycleDsp::OscillatorLaneCore::angleDelta(60, 0.f, 0.0) == 0.0);
}

TEST_CASE("Chained lane scheduling retains fractional cycle boundaries",
        "[cycle-dsp][oscillator-lane]") {
    CycleDsp::ChainedCycleState state;
    const double angleDelta = 1.0 / 100.25;

    CycleDsp::OscillatorLaneCore::advanceChainedCycle(state, angleDelta);
    REQUIRE(state.samplesThisCycle == 100);
    REQUIRE(state.sampledFrontier == 100);
    REQUIRE_THAT(state.cumulativePosition, WithinAbs(100.25, 1.0e-12));

    CycleDsp::OscillatorLaneCore::advanceChainedCycle(state, angleDelta);
    REQUIRE(state.samplesThisCycle == 100);
    REQUIRE(state.sampledFrontier == 200);
    REQUIRE_THAT(state.cumulativePosition, WithinAbs(200.5, 1.0e-12));

    CycleDsp::OscillatorLaneCore::advanceChainedCycle(state, angleDelta);
    REQUIRE(state.samplesThisCycle == 100);
    REQUIRE(state.sampledFrontier == 300);

    CycleDsp::OscillatorLaneCore::advanceChainedCycle(state, angleDelta);
    REQUIRE(state.samplesThisCycle == 101);
    REQUIRE(state.sampledFrontier == 401);
}
