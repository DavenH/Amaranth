#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include <Audio/AudioCallbackCapture.h>

TEST_CASE("Audio callback capture records stereo device blocks without allocation",
        "[audio][device][capture]") {
    AudioCallbackCapture capture;
    std::array<float, 256> left;
    std::array<float, 256> right;
    left.fill(0.25f);
    right.fill(-0.5f);
    float* channels[] { left.data(), right.data() };

    REQUIRE(capture.begin(48000.0, 10));
    capture.append(channels, 2, (int) left.size());
    capture.append(channels, 2, (int) left.size());
    const auto result = capture.waitForCompletion(10);

    REQUIRE(result.completed);
    REQUIRE(result.sampleRate == 48000.0);
    REQUIRE(result.left.size() == 480);
    REQUIRE(result.right.size() == 480);
    REQUIRE(result.firstCallback == 1);
    REQUIRE(result.lastCallback == 2);
    REQUIRE(result.peak == 0.5f);
    REQUIRE(result.rms > 0.39f);
    REQUIRE(result.rms < 0.40f);
    REQUIRE(std::all_of(result.left.begin(), result.left.end(), [](float sample) {
        return sample == 0.25f;
    }));
    REQUIRE(std::all_of(result.right.begin(), result.right.end(), [](float sample) {
        return sample == -0.5f;
    }));
}

TEST_CASE("Audio callback capture mirrors mono device output",
        "[audio][device][capture]") {
    AudioCallbackCapture capture;
    std::array<float, 64> mono;
    mono.fill(0.125f);
    float* channels[] { mono.data() };

    REQUIRE(capture.begin(8000.0, 8));
    capture.append(channels, 1, (int) mono.size());
    const auto result = capture.waitForCompletion(10);

    REQUIRE(result.completed);
    REQUIRE(result.left == result.right);
}

TEST_CASE("Cancelled audio callback capture does not report completion",
        "[audio][device][capture]") {
    AudioCallbackCapture capture;
    std::array<float, 32> mono;
    mono.fill(0.125f);
    float* channels[] { mono.data() };

    REQUIRE(capture.begin(8000.0, 8));
    capture.append(channels, 1, (int) mono.size());
    capture.cancel();
    const auto result = capture.waitForCompletion(10);

    REQUIRE_FALSE(result.completed);
    REQUIRE(result.left.empty());
    REQUIRE(result.right.empty());
}
