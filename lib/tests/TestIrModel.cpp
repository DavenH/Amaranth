#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/IrModel.h>

#include <array>

TEST_CASE("IR model mappings preserve Cycle 1 parameter behavior", "[cycle-dsp][ir]") {
    REQUIRE(CycleDsp::irImpulseLength(0.) == 128);
    REQUIRE(CycleDsp::irImpulseLength(1.) == 16384);
    REQUIRE(CycleDsp::irImpulseLengthValue(128) == Catch::Approx(0.));
    REQUIRE(CycleDsp::irImpulseLengthValue(16384) == Catch::Approx(1.));
    REQUIRE(CycleDsp::irPostGain(0.5) == Catch::Approx(1.f));
    REQUIRE(CycleDsp::irPrefilterAmount(0.5) == Catch::Approx(0.125f));
}

TEST_CASE("IR frequency prefilter preserves Cycle 1 endpoint behavior", "[cycle-dsp][ir]") {
    constexpr int length = 128;
    std::array<float, length> raw {};
    std::array<float, length> filtered {};
    std::array<float, length / 2> levels {};
    raw[0] = 1.f;
    raw[7] = -0.25f;

    Transform transform;
    transform.allocate(length, Transform::DivFwdByN, true);

    CycleDsp::buildIrPrefilterLevels({ levels.data(), (int) levels.size() }, 0.);
    CycleDsp::applyIrFrequencyPrefilter(
            { raw.data(), length },
            { filtered.data(), length },
            { levels.data(), (int) levels.size() },
            transform);

    for (int i = 0; i < length; ++i) {
        REQUIRE(filtered[(size_t) i] == Catch::Approx(raw[(size_t) i]).margin(1.0e-5f));
    }

    CycleDsp::buildIrPrefilterLevels({ levels.data(), (int) levels.size() }, 1.);
    CycleDsp::applyIrFrequencyPrefilter(
            { raw.data(), length },
            { filtered.data(), length },
            { levels.data(), (int) levels.size() },
            transform);

    for (float level : levels) {
        REQUIRE(level == 0.f);
    }

    for (int i = 1; i < length; ++i) {
        REQUIRE(filtered[(size_t) i] == Catch::Approx(filtered[0]).margin(1.0e-6f));
    }
}
