#include <Audio/CycleDsp/CycleDelay.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using CycleDsp::CycleDelay;
using CycleDsp::DelayChannel;
using CycleDsp::DelayConfiguration;

namespace {

DelayConfiguration testConfiguration() {
    DelayConfiguration configuration;
    configuration.sampleRate = 16.0;
    configuration.delaySeconds = 0.2;
    configuration.feedback = 0.5f;
    configuration.spin = 0.f;
    configuration.wet = 1.f;
    configuration.spinIterations = 1;
    return configuration;
}

}

TEST_CASE("CycleDelay preserves delay history across blocks", "[CycleDelay]") {
    CycleDelay delay;
    delay.configure(testConfiguration());

    std::array<float, 3> first { 1.f, 0.f, 0.f };
    std::array<float, 4> second {};
    delay.process({ first.data(), (int) first.size() });
    delay.process({ second.data(), (int) second.size() });

    REQUIRE(first[0] == Catch::Approx(1.f));
    REQUIRE(second[0] == Catch::Approx(1.f));
}

TEST_CASE("CycleDelay applies feedback after the first wet tap", "[CycleDelay]") {
    auto lowFeedbackConfiguration = testConfiguration();
    lowFeedbackConfiguration.sampleRate = 10.0;
    lowFeedbackConfiguration.delaySeconds = 0.1;
    lowFeedbackConfiguration.feedback = 0.2f;
    lowFeedbackConfiguration.wet = 0.4f;
    auto highFeedbackConfiguration = lowFeedbackConfiguration;
    highFeedbackConfiguration.feedback = 0.8f;

    CycleDelay lowFeedbackDelay;
    CycleDelay highFeedbackDelay;
    lowFeedbackDelay.configure(lowFeedbackConfiguration);
    highFeedbackDelay.configure(highFeedbackConfiguration);
    std::array<float, 3> lowFeedback { 1.f, 0.f, 0.f };
    std::array<float, 3> highFeedback = lowFeedback;
    lowFeedbackDelay.process({ lowFeedback.data(), (int) lowFeedback.size() });
    highFeedbackDelay.process({ highFeedback.data(), (int) highFeedback.size() });

    REQUIRE(lowFeedback[1] == Catch::Approx(0.4f));
    REQUIRE(highFeedback[1] == Catch::Approx(0.4f));
    REQUIRE(lowFeedback[2] == Catch::Approx(0.08f));
    REQUIRE(highFeedback[2] == Catch::Approx(0.32f));
}

TEST_CASE("CycleDelay is independent of block partitioning", "[CycleDelay]") {
    CycleDelay contiguousDelay;
    CycleDelay partitionedDelay;
    contiguousDelay.configure(testConfiguration());
    partitionedDelay.configure(testConfiguration());

    std::vector<float> contiguous(16, 0.f);
    std::vector<float> partitioned(16, 0.f);
    contiguous.front() = 1.f;
    partitioned.front() = 1.f;

    contiguousDelay.process({ contiguous.data(), (int) contiguous.size() });
    partitionedDelay.process({ partitioned.data(), 3 });
    partitionedDelay.process({ partitioned.data() + 3, 5 });
    partitionedDelay.process({ partitioned.data() + 8, 8 });

    REQUIRE(partitioned == contiguous);
}

TEST_CASE("CycleDelay reset clears execution state", "[CycleDelay]") {
    CycleDelay delay;
    CycleDelay freshDelay;
    delay.configure(testConfiguration());
    freshDelay.configure(testConfiguration());

    std::array<float, 4> impulse { 1.f, 0.f, 0.f, 0.f };
    delay.process({ impulse.data(), (int) impulse.size() });
    delay.reset();

    std::array<float, 8> silence {};
    std::array<float, 8> freshSilence {};
    delay.process({ silence.data(), (int) silence.size() });
    freshDelay.process({ freshSilence.data(), (int) freshSilence.size() });

    REQUIRE(silence == freshSilence);
}

TEST_CASE("CycleDelay mapping preserves Cycle tempo and spin semantics", "[CycleDelay]") {
    REQUIRE(CycleDsp::delayTimeSeconds(0.5, 120.0, 4) == Catch::Approx(0.5));
    REQUIRE(CycleDsp::delayTimeSeconds(0.0, 60.0, 3) == Catch::Approx(0.0675));
    REQUIRE(CycleDsp::delaySpinIterations(0.0) == 1);
    REQUIRE(CycleDsp::delaySpinIterations(0.5) == 3);
    REQUIRE(CycleDsp::delaySpinIterations(1.0) == 12);
}

TEST_CASE("CycleDelay does not alias power-of-two delay lengths", "[CycleDelay]") {
    CycleDelay delay;
    auto configuration = testConfiguration();
    configuration.delaySeconds = 0.25;
    delay.configure(configuration);

    std::array<float, 5> samples { 1.f, 0.f, 0.f, 0.f, 0.f };
    delay.process({ samples.data(), (int) samples.size() });

    REQUIRE(samples[0] == Catch::Approx(1.f));
    REQUIRE(samples[1] == Catch::Approx(0.f).margin(1e-15f));
    REQUIRE(samples[2] == Catch::Approx(0.f).margin(1e-15f));
    REQUIRE(samples[3] == Catch::Approx(0.f).margin(1e-15f));
    REQUIRE(samples[4] == Catch::Approx(1.f));
}

TEST_CASE("CycleDelay preserves stereo spin panning", "[CycleDelay]") {
    CycleDelay leftDelay;
    CycleDelay rightDelay;
    auto leftConfiguration = testConfiguration();
    leftConfiguration.sampleRate = 20.0;
    leftConfiguration.delaySeconds = 0.1;
    leftConfiguration.spin = 1.f;
    leftConfiguration.spinIterations = 4;
    leftConfiguration.channel = DelayChannel::Left;
    auto rightConfiguration = leftConfiguration;
    rightConfiguration.channel = DelayChannel::Right;
    leftDelay.configure(leftConfiguration);
    rightDelay.configure(rightConfiguration);

    std::array<float, 9> left { 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    auto right = left;
    leftDelay.process({ left.data(), (int) left.size() });
    rightDelay.process({ right.data(), (int) right.size() });

    REQUIRE(left[2] == Catch::Approx(1.f));
    REQUIRE(right[2] == Catch::Approx(1.f));
    REQUIRE(left[4] < 0.001f);
    REQUIRE(right[4] == Catch::Approx(0.5f).margin(0.0001f));
    REQUIRE(left != right);
}
