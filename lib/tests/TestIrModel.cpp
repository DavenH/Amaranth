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

TEST_CASE("IR curve-to-kernel path preserves Cycle 1 golden samples", "[cycle-dsp][ir]") {
    constexpr int length = 128;
    constexpr int oversampledLength = length * 2;
    std::array<float, 2> waveX { -1.f, 2.f };
    std::array<float, 2> waveY { 0.25f, 0.25f };
    std::array<float, 2> slope {};
    std::array<float, 2> area {};
    std::array<float, length> raw {};
    std::array<float, length> filtered {};
    std::array<float, oversampledLength> scratch {};
    std::array<float, length / 2> levels {};

    Rasterization::WaveformBuffers waveform(
            { waveX.data(), (int) waveX.size() },
            { waveY.data(), (int) waveY.size() },
            {},
            { slope.data(), (int) slope.size() },
            { area.data(), (int) area.size() },
            0,
            1);
    Oversampler oversampler(8);
    oversampler.setOversampleFactor(2);
    oversampler.setMemoryBuffer({ scratch.data(), (int) scratch.size() });

    CycleDsp::rasterizeIrImpulse(
            Rasterization::SamplerView(waveform, true),
            { raw.data(), length },
            oversampler,
            0.0625);

    Transform transform;
    transform.allocate(length, Transform::DivFwdByN, true);
    CycleDsp::buildIrPrefilterLevels({ levels.data(), (int) levels.size() }, 0.5);
    CycleDsp::applyIrFrequencyPrefilter(
            { raw.data(), length },
            { filtered.data(), length },
            { levels.data(), (int) levels.size() },
            transform);

    constexpr std::array<int, 10> indices { 0, 1, 2, 3, 7, 15, 31, 63, 95, 127 };
    constexpr std::array<float, 10> expected {
            0.112169f,
            0.119890f,
            0.116117f,
            0.126807f,
            0.286509f,
            0.228395f,
            0.239318f,
            0.245683f,
            0.251212f,
            0.348778f
    };

    for (size_t i = 0; i < indices.size(); ++i) {
        REQUIRE(filtered[(size_t) indices[i]] == Catch::Approx(expected[i]).margin(1.0e-5f));
    }
}
