#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/SpectralLayerCore.h>

#include <array>

using Catch::Approx;

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
