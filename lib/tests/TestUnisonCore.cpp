#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Audio/CycleDsp/UnisonCore.h>

using Catch::Matchers::WithinAbs;

TEST_CASE("Unison group layout preserves Cycle 1 voice generation", "[unison][dsp]") {
    CycleDsp::UnisonGroupConfiguration configuration;
    configuration.order = 3;
    configuration.detuneWidthCents = 70.f;
    configuration.panSpread = 1.f;
    configuration.phaseSpread = 0.5f;
    configuration.jitter = 0.8f;

    const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);

    REQUIRE(layout.order == 3);
    REQUIRE_THAT(layout[0].detuneCents, WithinAbs(-63.952, 0.0001));
    REQUIRE_THAT(layout[1].detuneCents, WithinAbs(3.416, 0.0001));
    REQUIRE_THAT(layout[2].detuneCents, WithinAbs(60.872, 0.0001));
    REQUIRE_THAT(layout[0].pan, WithinAbs(1.0, 0.0001));
    REQUIRE_THAT(layout[1].pan, WithinAbs(0.5, 0.0001));
    REQUIRE_THAT(layout[2].pan, WithinAbs(0.0, 0.0001));
    REQUIRE_THAT(layout[0].phaseCycles, WithinAbs(0.7901333, 0.0001));
    REQUIRE_THAT(layout[1].phaseCycles, WithinAbs(0.9756, 0.0001));
    REQUIRE_THAT(layout[2].phaseCycles, WithinAbs(0.2318667, 0.0001));
}

TEST_CASE("Unison bypass resolves to the neutral voice", "[unison][dsp]") {
    CycleDsp::UnisonGroupConfiguration configuration;
    configuration.order = CycleDsp::maximumUnisonOrder;
    configuration.enabled = false;

    const auto layout = CycleDsp::UnisonCore::makeGroupLayout(configuration);

    REQUIRE(layout.order == 1);
    REQUIRE(layout[0].detuneCents == 0.f);
    REQUIRE(layout[0].phaseCycles == 0.f);
    REQUIRE(layout[0].pan == 0.5f);
}

TEST_CASE("Unison panning preserves even and odd Cycle 1 alternation", "[unison][dsp]") {
    CycleDsp::UnisonGroupConfiguration configuration;
    configuration.order = 4;
    const auto even = CycleDsp::UnisonCore::makeGroupLayout(configuration);

    REQUIRE(even[0].pan == 1.f);
    REQUIRE(even[1].pan == 0.f);
    REQUIRE(even[2].pan == 1.f);
    REQUIRE(even[3].pan == 0.f);

    configuration.order = 5;
    const auto odd = CycleDsp::UnisonCore::makeGroupLayout(configuration);
    REQUIRE(odd[0].pan == 1.f);
    REQUIRE(odd[1].pan == 0.f);
    REQUIRE(odd[2].pan == 0.5f);
    REQUIRE(odd[3].pan == 1.f);
    REQUIRE(odd[4].pan == 0.f);
}

TEST_CASE("Unison parameter mappings retain Cycle 1 endpoints", "[unison][mapping]") {
    REQUIRE(CycleDsp::UnisonCore::orderFromUnitValue(0.0) == 1);
    REQUIRE(CycleDsp::UnisonCore::orderFromUnitValue(0.5) == 6);
    REQUIRE(CycleDsp::UnisonCore::orderFromUnitValue(1.0) == 10);
    REQUIRE(CycleDsp::UnisonCore::detuneCentsFromPosition(0.f, 70.f) == -70.f);
    REQUIRE(CycleDsp::UnisonCore::detuneCentsFromPosition(0.5f, 70.f) == 0.f);
    REQUIRE(CycleDsp::UnisonCore::detuneCentsFromPosition(1.f, 70.f) == 70.f);
    REQUIRE_THAT(
            CycleDsp::UnisonCore::voiceLevelScale(10),
            WithinAbs(0.41754395, 0.000001));
}

TEST_CASE("Unison phase trajectories reflect pitch detune duration and wrapping",
        "[unison][dsp][preview]") {
    const auto flat = CycleDsp::UnisonCore::phaseTrajectory(60, 0.f, 0.f);
    const auto upward = CycleDsp::UnisonCore::phaseTrajectory(60, 5.f, 0.f);
    const auto downward = CycleDsp::UnisonCore::phaseTrajectory(60, -5.f, 0.f);
    const auto octaveUp = CycleDsp::UnisonCore::phaseTrajectory(72, 5.f, 0.f);

    REQUIRE(flat.driftCyclesPerSecond == 0.0);
    REQUIRE(upward.driftCyclesPerSecond > 0.0);
    REQUIRE(downward.driftCyclesPerSecond < 0.0);
    REQUIRE_THAT(
            octaveUp.driftCyclesPerSecond,
            WithinAbs(upward.driftCyclesPerSecond * 2.0, 0.000001));
    REQUIRE_THAT(
            upward.phaseAt(2.0),
            WithinAbs(CycleDsp::UnisonCore::wrapSignedPhase(
                    upward.driftCyclesPerSecond * 2.0), 0.000001));
    REQUIRE_THAT(CycleDsp::UnisonCore::wrapSignedPhase(0.5), WithinAbs(-0.5, 0.000001));
    REQUIRE_THAT(CycleDsp::UnisonCore::wrapSignedPhase(-0.5), WithinAbs(-0.5, 0.000001));
}

TEST_CASE("Unison phase segments split exactly at positive and negative wraps",
        "[unison][dsp][preview]") {
    const CycleDsp::UnisonPhaseTrajectory upward { 0.4f, 0.2 };
    const CycleDsp::UnisonPhaseTrajectory downward { -0.4f, -0.2 };

    const auto rising = CycleDsp::UnisonCore::phaseSegments(upward, 1.0);
    const auto falling = CycleDsp::UnisonCore::phaseSegments(downward, 1.0);

    REQUIRE(rising.size() == 2);
    REQUIRE_THAT(rising[0].endSeconds, WithinAbs(0.5, 0.000001));
    REQUIRE_THAT(rising[0].endPhaseCycles, WithinAbs(0.5, 0.000001));
    REQUIRE_THAT(rising[1].startPhaseCycles, WithinAbs(-0.5, 0.000001));
    REQUIRE_THAT(rising[1].endPhaseCycles, WithinAbs(-0.4, 0.000001));

    REQUIRE(falling.size() == 2);
    REQUIRE_THAT(falling[0].endSeconds, WithinAbs(0.5, 0.000001));
    REQUIRE_THAT(falling[0].endPhaseCycles, WithinAbs(-0.5, 0.000001));
    REQUIRE_THAT(falling[1].startPhaseCycles, WithinAbs(0.5, 0.000001));
    REQUIRE_THAT(falling[1].endPhaseCycles, WithinAbs(0.4, 0.000001));

    const CycleDsp::UnisonPhaseTrajectory exactBoundary { 0.25f, 0.5 };
    const auto exact = CycleDsp::UnisonCore::phaseSegments(exactBoundary, 0.5);
    REQUIRE(exact.size() == 1);
    REQUIRE_THAT(exact.front().endSeconds, WithinAbs(0.5, 0.000001));
    REQUIRE_THAT(exact.front().endPhaseCycles, WithinAbs(0.5, 0.000001));
}
