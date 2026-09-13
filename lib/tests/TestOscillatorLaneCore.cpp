#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Audio/CycleDsp/CyclicFrameLaneRenderer.h>
#include <Audio/CycleDsp/OscillatorLaneCore.h>
#include <Util/NumberUtils.h>

using Catch::Matchers::WithinAbs;

TEST_CASE("Oscillator lane pitch preserves the Cycle 1 angle-delta contract",
        "[cycle-dsp][oscillator-lane]") {
    const double middleC = CycleDsp::OscillatorLaneCore::angleDelta(60, 0.f, 44100.0);
    const double octave = CycleDsp::OscillatorLaneCore::angleDelta(72, 0.f, 44100.0);
    const double detuned = CycleDsp::OscillatorLaneCore::angleDelta(60, 1200.f, 44100.0);

    REQUIRE_THAT(octave, WithinAbs(middleC * 2.0, 1.0e-12));
    REQUIRE_THAT(detuned, WithinAbs(middleC * 2.0, 1.0e-12));
    REQUIRE_THAT(
            CycleDsp::OscillatorLaneCore::angleDeltaForPitchUnit(
                    60, 0.f, 0.75f, 44100.0),
            WithinAbs(
                    CycleDsp::OscillatorLaneCore::angleDelta(66, 0.f, 44100.0),
                    1.0e-12));
    REQUIRE(CycleDsp::OscillatorLaneCore::angleDelta(60, 0.f, 0.0) == 0.0);
}

TEST_CASE("Oscillator neutral clock retains the Cycle 1 frequency precision",
        "[cycle-dsp][oscillator-lane][parity]") {
    const float legacyFrequency = (float) NumberUtils::noteToFrequency(48);

    REQUIRE(CycleDsp::OscillatorLaneCore::legacyNeutralAngleDelta(36, 44100.0)
            == (double) legacyFrequency / 44100.0);
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

TEST_CASE("Control frame stride preserves the Cycle 1 cycle-clocked contract",
        "[cycle-dsp][oscillator-lane][control-rate][parity]") {
    REQUIRE(CycleDsp::OscillatorLaneCore::controlFrameStride(16, 168.5) == 1);
    REQUIRE(CycleDsp::OscillatorLaneCore::controlFrameStride(256, 168.5) == 2);
    REQUIRE(CycleDsp::OscillatorLaneCore::controlFrameStride(1024, 168.5) == 6);
    REQUIRE(CycleDsp::OscillatorLaneCore::controlFrameStride(0, 168.5) == 1);
    REQUIRE(CycleDsp::OscillatorLaneCore::controlFrameStride(256, 0.0) == 1);
}

TEST_CASE("Interpolated frame position preserves the Cycle 1 precision boundary",
        "[cycle-dsp][oscillator-lane][interpolation][parity]") {
    REQUIRE(CycleDsp::OscillatorLaneCore::interpolatedFramePortion(
            true,
            5,
            4,
            1000.0,
            750.0,
            500.0) == 0.25f);

    const double futurePosition = 25575.57837117975;
    const double lanePosition = 25322.469779209365;
    const double frameInterval = 331.39172955174166;
    const float legacyPortion = (float) (1.0
            - (futurePosition - lanePosition) / (float) frameInterval);
    const float doubleIntervalPortion = (float) (1.0
            - (futurePosition - lanePosition) / frameInterval);
    REQUIRE(legacyPortion != doubleIntervalPortion);
    REQUIRE(CycleDsp::OscillatorLaneCore::interpolatedFramePortion(
            false,
            31,
            2,
            futurePosition,
            lanePosition,
            frameInterval) == legacyPortion);
}

TEST_CASE("Shared spectral scheduling preserves Cycle 1 lane saturation",
        "[cycle-dsp][oscillator-lane][scheduling][parity]") {
    REQUIRE_FALSE(CycleDsp::OscillatorLaneCore::sharedFrameSaturated(
            false, 0, 0, 999.99, 1000.0));
    REQUIRE(CycleDsp::OscillatorLaneCore::sharedFrameSaturated(
            false, 0, 0, 1000.0, 1000.0));
    REQUIRE(CycleDsp::OscillatorLaneCore::laneWithinSharedFrame(
            false, 0, 0, 999.99, 1000.0));
    REQUIRE_FALSE(CycleDsp::OscillatorLaneCore::laneWithinSharedFrame(
            false, 0, 0, 1000.0, 1000.0));

    REQUIRE_FALSE(CycleDsp::OscillatorLaneCore::sharedFrameSaturated(
            true, 3, 4, 2000.0, 1000.0));
    REQUIRE(CycleDsp::OscillatorLaneCore::sharedFrameSaturated(
            true, 4, 4, 0.0, 1000.0));
    REQUIRE(CycleDsp::OscillatorLaneCore::laneWithinSharedFrame(
            true, 3, 4, 2000.0, 1000.0));
    REQUIRE_FALSE(CycleDsp::OscillatorLaneCore::laneWithinSharedFrame(
            true, 4, 4, 0.0, 1000.0));
}

TEST_CASE("Cyclic frame composition preserves the unshifted first cycle",
        "[cycle-dsp][oscillator-lane][cyclic-frame]") {
    float currentData[] { 10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f };
    float previousData[] { 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f };
    float fadeInData[] { 0.f, 0.25f, 0.75f, 1.f };
    float fadeOutData[] { 1.f, 0.75f, 0.25f, 0.f };
    float biasedData[8] {};
    float shiftedCurrentData[8] {};
    float shiftedPreviousData[8] {};
    float previousHalfData[4] {};
    float lastLerpHalfData[4] {};

    const auto composed = CycleDsp::CyclicFrameLaneRenderer::compose(
            { Buffer<float>(currentData, 8),
                    Buffer<float>(previousData, 8),
                    Buffer<float>(fadeInData, 4),
                    Buffer<float>(fadeOutData, 4),
                    0.25f,
                    0.f,
                    true,
                    false },
            { Buffer<float>(lastLerpHalfData, 4) },
            { Buffer<float>(biasedData, 8),
                    Buffer<float>(shiftedCurrentData, 8),
                    Buffer<float>(shiftedPreviousData, 8),
                    Buffer<float>(previousHalfData, 4) });

    REQUIRE(composed.size() == 8);
    for (int i = 0; i < composed.size(); ++i) {
        REQUIRE(composed[i] == previousData[i]);
    }
}

TEST_CASE("Cyclic frame fades preserve the Cycle 1 half-frame contract",
        "[cycle-dsp][oscillator-lane][cyclic-frame]") {
    float fadeInData[4] {};
    float fadeOutData[4] {};
    REQUIRE(CycleDsp::CyclicFrameLaneRenderer::makeHalfFrameFades(
            8,
            Buffer<float>(fadeInData, 4),
            Buffer<float>(fadeOutData, 4)));

    REQUIRE(fadeInData[0] == Catch::Approx(0.f).margin(1.0e-6f));
    REQUIRE(fadeInData[3] == Catch::Approx(1.f).margin(1.0e-6f));
    for (int i = 0; i < 4; ++i) {
        REQUIRE(fadeInData[i] + fadeOutData[i]
                == Catch::Approx(1.f).margin(1.0e-6f));
    }
}

TEST_CASE("Cyclic frame composition rotates and seeds the first shifted cycle",
        "[cycle-dsp][oscillator-lane][cyclic-frame]") {
    float currentData[] { 10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f };
    float previousData[] { 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f };
    float fadeInData[] { 0.f, 0.25f, 0.75f, 1.f };
    float fadeOutData[] { 1.f, 0.75f, 0.25f, 0.f };
    float biasedData[8] {};
    float shiftedCurrentData[8] {};
    float shiftedPreviousData[8] {};
    float previousHalfData[4] {};
    float lastLerpHalfData[4] {};
    const float expected[] { 6.f, 7.f, 0.f, 1.f, 2.f, 3.f, 4.f, 5.f };

    const auto composed = CycleDsp::CyclicFrameLaneRenderer::compose(
            { Buffer<float>(currentData, 8),
                    Buffer<float>(previousData, 8),
                    Buffer<float>(fadeInData, 4),
                    Buffer<float>(fadeOutData, 4),
                    0.25f,
                    0.f,
                    true,
                    true },
            { Buffer<float>(lastLerpHalfData, 4) },
            { Buffer<float>(biasedData, 8),
                    Buffer<float>(shiftedCurrentData, 8),
                    Buffer<float>(shiftedPreviousData, 8),
                    Buffer<float>(previousHalfData, 4) });

    for (int i = 0; i < composed.size(); ++i) {
        REQUIRE(composed[i] == expected[i]);
    }
    for (int i = 0; i < 4; ++i) {
        REQUIRE(lastLerpHalfData[i] == expected[i]);
    }
}

TEST_CASE("Cyclic frame composition interpolates and crossfades subsequent cycles",
        "[cycle-dsp][oscillator-lane][cyclic-frame]") {
    float currentData[] { 10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f };
    float previousData[] { 0.f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f };
    float fadeInData[] { 0.f, 0.25f, 0.75f, 1.f };
    float fadeOutData[] { 1.f, 0.75f, 0.25f, 0.f };
    float biasedData[8] {};
    float shiftedCurrentData[8] {};
    float shiftedPreviousData[8] {};
    float previousHalfData[4] {};
    float lastLerpHalfData[] { 1.f, 2.f, 3.f, 4.f };
    const float expected[] { 1.f, 3.f, 6.f, 8.f, 9.f, 10.f, 11.f, 12.f };
    const float expectedNextHalf[] { 5.f, 6.f, 7.f, 8.f };

    const auto composed = CycleDsp::CyclicFrameLaneRenderer::compose(
            { Buffer<float>(currentData, 8),
                    Buffer<float>(previousData, 8),
                    Buffer<float>(fadeInData, 4),
                    Buffer<float>(fadeOutData, 4),
                    0.f,
                    0.5f,
                    false,
                    false },
            { Buffer<float>(lastLerpHalfData, 4) },
            { Buffer<float>(biasedData, 8),
                    Buffer<float>(shiftedCurrentData, 8),
                    Buffer<float>(shiftedPreviousData, 8),
                    Buffer<float>(previousHalfData, 4) });

    for (int i = 0; i < composed.size(); ++i) {
        REQUIRE(composed[i] == expected[i]);
    }
    for (int i = 0; i < 4; ++i) {
        REQUIRE(lastLerpHalfData[i] == expectedNextHalf[i]);
    }
}
