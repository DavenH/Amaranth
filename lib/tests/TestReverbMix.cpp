#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Audio/CycleDsp/ReverbMix.h>

#include <array>

TEST_CASE("Shared reverb mix preserves the Cycle stereo width law",
        "[audio][reverb][parity]") {
    std::array<float, 3> dry { 1.f, -0.5f, 0.25f };
    std::array<float, 3> directWet { 0.8f, 0.4f, -0.2f };
    std::array<float, 3> crossWet { -0.6f, 0.2f, 0.5f };
    std::array<float, 3> output {};
    constexpr float wetLevel = 0.2f;
    constexpr float width = 0.25f;

    CycleDsp::mixReverbChannel(
            { dry.data(), (int) dry.size() },
            { directWet.data(), (int) directWet.size() },
            { crossWet.data(), (int) crossWet.size() },
            { output.data(), (int) output.size() },
            wetLevel,
            width);

    const float direct = wetLevel * 0.5f;
    const float cross = wetLevel * 0.5f;
    const float dryScale = 1.f - 0.24f * wetLevel;
    for (size_t sample = 0; sample < output.size(); ++sample) {
        REQUIRE(output[sample] == Catch::Approx(
                dry[sample] * dryScale
                + directWet[sample] * direct
                + crossWet[sample] * cross));
    }
}

TEST_CASE("Shared mono reverb mix preserves the Cycle dry law",
        "[audio][reverb][parity]") {
    std::array<float, 2> dry { 1.f, -0.5f };
    std::array<float, 2> wet { 0.25f, 0.75f };
    std::array<float, 2> output {};
    constexpr float wetLevel = 0.4f;

    CycleDsp::mixReverbMono(
            { dry.data(), (int) dry.size() },
            { wet.data(), (int) wet.size() },
            { output.data(), (int) output.size() },
            wetLevel);

    REQUIRE(output[0] == Catch::Approx(1.f * 0.9f + 0.25f * wetLevel));
    REQUIRE(output[1] == Catch::Approx(-0.5f * 0.9f + 0.75f * wetLevel));
}
