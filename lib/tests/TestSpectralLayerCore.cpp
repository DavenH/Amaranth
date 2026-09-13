#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/SpectralLayerCore.h>

#include <array>

using Catch::Approx;

TEST_CASE("Spectral bin limits clear stale magnitude and phase tails",
        "[CycleDsp][spectral][bins]") {
    std::array<float, 6> magnitudes { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f };
    std::array<float, 6> phases { 6.f, 5.f, 4.f, 3.f, 2.f, 1.f };

    CycleDsp::SpectralLayerCore::clearBinsAbove(
            { magnitudes.data(), (int) magnitudes.size() },
            { phases.data(), (int) phases.size() },
            3);

    REQUIRE(magnitudes == (std::array<float, 6> { 1.f, 2.f, 3.f, 0.f, 0.f, 0.f }));
    REQUIRE(phases == (std::array<float, 6> { 6.f, 5.f, 4.f, 0.f, 0.f, 0.f }));
}

TEST_CASE("Spectral bin limits accept every available bin",
        "[CycleDsp][spectral][bins]") {
    std::array<float, 3> magnitudes { 1.f, 2.f, 3.f };
    std::array<float, 3> phases { 3.f, 2.f, 1.f };

    CycleDsp::SpectralLayerCore::clearBinsAbove(
            { magnitudes.data(), (int) magnitudes.size() },
            { phases.data(), (int) phases.size() },
            (int) magnitudes.size());

    REQUIRE(magnitudes == (std::array<float, 3> { 1.f, 2.f, 3.f }));
    REQUIRE(phases == (std::array<float, 3> { 3.f, 2.f, 1.f }));
}

TEST_CASE("Spectral phase layers pan their scaled offsets before accumulation",
        "[CycleDsp][spectral][phase][pan]") {
    std::array<float, 3> source { 0.25f, 0.5f, 0.75f };
    std::array<float, 3> left {};
    std::array<float, 3> right {};

    CycleDsp::SpectralLayerCore::renderPhaseChannels(
            { source.data(), (int) source.size() },
            { left.data(), (int) left.size() },
            { right.data(), (int) right.size() },
            1.f,
            0.f);

    for (size_t index = 0; index < source.size(); ++index) {
        REQUIRE(left[index] == 0.f);
        REQUIRE(right[index] == Approx(
                source[index] * MathConstants<float>::twoPi));
    }
}

TEST_CASE("Spectral phase harmonic scale follows the legacy square-root ramp",
        "[CycleDsp][spectral][phase]") {
    std::array<float, 4> scale {};

    CycleDsp::SpectralLayerCore::preparePhaseHarmonicScale(
            { scale.data(), (int) scale.size() });

    REQUIRE(scale[0] == 1.f);
    REQUIRE(scale[1] == Approx(std::sqrt(2.f)));
    REQUIRE(scale[2] == Approx(std::sqrt(3.f)));
    REQUIRE(scale[3] == 2.f);
}

TEST_CASE("Multiplicative spectral pan preserves the neutral magnitude",
        "[CycleDsp][spectral][magnitude][pan]") {
    std::array<float, 3> source { 0.25f, 0.5f, 0.75f };
    std::array<float, 3> left {};
    std::array<float, 3> right {};

    CycleDsp::SpectralLayerCore::renderMagnitudeChannels(
            { source.data(), (int) source.size() },
            { left.data(), (int) left.size() },
            { right.data(), (int) right.size() },
            0.f,
            0.5f,
            false);

    REQUIRE(left != right);
    REQUIRE(right[0] == Approx(1.f));
    REQUIRE(right[1] == Approx(1.f));
    REQUIRE(right[2] == Approx(1.f));
}

TEST_CASE("Additive spectral pan scales the layer contribution",
        "[CycleDsp][spectral][magnitude][pan]") {
    std::array<float, 3> source { 0.25f, 0.5f, 0.75f };
    std::array<float, 3> left {};
    std::array<float, 3> right {};

    CycleDsp::SpectralLayerCore::renderMagnitudeChannels(
            { source.data(), (int) source.size() },
            { left.data(), (int) left.size() },
            { right.data(), (int) right.size() },
            1.f,
            0.5f,
            true);

    REQUIRE(left == (std::array<float, 3> {}));
    REQUIRE(right != (std::array<float, 3> {}));
}

TEST_CASE("Spectral magnitude operands use arithmetic-specific polarity transfers",
        "[CycleDsp][spectral][magnitude][transfer]") {
    constexpr int harmonicCount = 4;
    constexpr float range = 1.f / 3.f;
    const float additiveScale = Arithmetic::calcAdditiveScaling(harmonicCount);
    std::array<float, 3> addUnipolar { 0.f, 0.5f, 1.f };
    std::array<float, 3> addBipolar = addUnipolar;
    std::array<float, 3> multiplyUnipolar = addUnipolar;
    std::array<float, 3> multiplyBipolar = addUnipolar;

    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            { addUnipolar.data(), (int) addUnipolar.size() },
            range,
            true,
            false,
            harmonicCount);
    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            { addBipolar.data(), (int) addBipolar.size() },
            range,
            true,
            true,
            harmonicCount);
    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            { multiplyUnipolar.data(), (int) multiplyUnipolar.size() },
            range,
            false,
            false,
            harmonicCount);
    CycleDsp::SpectralLayerCore::shapeMagnitudeOperand(
            { multiplyBipolar.data(), (int) multiplyBipolar.size() },
            range,
            false,
            true,
            harmonicCount);

    REQUIRE(addUnipolar[0] == Approx(0.f).margin(1.0e-8f));
    REQUIRE(addUnipolar[1] == Approx(additiveScale));
    REQUIRE(addUnipolar[2] == Approx(2.f * additiveScale));
    REQUIRE(addBipolar[0] == Approx(-additiveScale));
    REQUIRE(addBipolar[1] == Approx(0.f));
    REQUIRE(addBipolar[2] == Approx(additiveScale));
    REQUIRE(multiplyUnipolar[0] == Approx(0.f).margin(1.0e-8f));
    REQUIRE(multiplyUnipolar[1] == Approx(0.5f));
    REQUIRE(multiplyUnipolar[2] == Approx(1.f));
    REQUIRE(multiplyBipolar[0] == Approx(0.f).margin(1.0e-8f));
    REQUIRE(multiplyBipolar[1] == Approx(1.f));
    REQUIRE(multiplyBipolar[2] == Approx(2.f));
}
