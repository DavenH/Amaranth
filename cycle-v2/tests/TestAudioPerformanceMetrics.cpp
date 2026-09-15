#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Runtime/AudioPerformanceMetrics.h"

using namespace CycleV2;
using namespace juce;

namespace {

const var& property(const var& value, const Identifier& name) {
    return value.getDynamicObject()->getProperties()[name];
}

AudioPerformanceMetrics::RealtimeSample beginSample(
        AudioPerformanceMetrics& metrics) {
    AudioPerformanceMetrics::RealtimeSample sample;
    REQUIRE(metrics.beginRealtimeSample(sample, 256, 48'000.0));
    return sample;
}

}

TEST_CASE("Audio performance metrics aggregate realtime samples off-thread",
        "[cycle-v2][audio][performance]") {
    AudioPerformanceMetrics metrics;
    REQUIRE_FALSE(metrics.snapshot().enabled);
    metrics.resetAndEnable();

    auto first = beginSample(metrics);
    first.callbackDurationMicroseconds = 2'000;
    first.deadlineMicroseconds = 5'000;
    first.graphRevision = 17;
    first.activeVoiceCount = 2;
    first.scheduledMidiEventCount = 3;
    first.executionStepCount = 21;
    first.executionStepVisitCount = 15;
    first.modulationBindingVisitCount = 2;
    first.spectralTransferBindingVisitCount = 5;
    first.stageDurations[static_cast<size_t>(
            AudioPerformanceMetrics::Stage::VoiceRendering)] = 1'200;
    metrics.publishRealtimeSample(first);

    auto second = beginSample(metrics);
    second.callbackDurationMicroseconds = 6'000;
    second.deadlineMicroseconds = 5'000;
    second.graphRevision = 18;
    second.activeVoiceCount = 4;
    second.executionStepCount = 22;
    second.executionStepVisitCount = 30;
    second.modulationBindingVisitCount = 4;
    second.spectralTransferBindingVisitCount = 7;
    second.stageDurations[static_cast<size_t>(
            AudioPerformanceMetrics::Stage::VoiceRendering)] = 4'500;
    metrics.publishRealtimeSample(second);

    REQUIRE(metrics.snapshot().callbackDuration.count == 0);
    metrics.serviceNonRealtime();
    const auto snapshot = metrics.snapshot();
    REQUIRE(snapshot.callbackDuration.count == 2);
    REQUIRE(snapshot.callbackDuration.totalMicroseconds == 8'000);
    REQUIRE(snapshot.deadlineOverruns == 1);
    REQUIRE(snapshot.totalFrames == 512);
    REQUIRE(snapshot.totalVoiceBlocks == 6);
    REQUIRE(snapshot.totalExecutionStepVisits == 45);
    REQUIRE(snapshot.totalModulationBindingVisits == 6);
    REQUIRE(snapshot.totalSpectralTransferBindingVisits == 12);
    REQUIRE(snapshot.latestGraphRevision == 18);
    REQUIRE(snapshot.maximumActiveVoiceCount == 4);
    REQUIRE(snapshot.maximumScheduledMidiEventCount == 3);
    REQUIRE(snapshot.maximumExecutionStepCount == 22);
    REQUIRE(snapshot.stages[static_cast<size_t>(
            AudioPerformanceMetrics::Stage::VoiceRendering)]
                    .totalMicroseconds == 5'700);

    const var exported = metrics.toVar();
    REQUIRE(property(exported, "schema").toString()
            == "cycle-v2-audio-performance.v1");
    REQUIRE((int64) property(exported, "callbackCount") == 2);
    REQUIRE((int64) property(exported, "deadlineOverruns") == 1);
    REQUIRE((double) property(
            property(exported, "callbackDuration"),
            "meanMs") == Catch::Approx(4.0));
    REQUIRE((double) property(
            property(exported, "deadlineUtilization"),
            "meanPercent") == Catch::Approx(80.0));
    REQUIRE((int64) property(
            property(exported, "workload"),
            "totalVoiceBlocks") == 6);
    REQUIRE((double) property(
            property(exported, "workload"),
            "meanExecutionStepVisits") == Catch::Approx(22.5));
    REQUIRE((double) property(
            property(exported, "workload"),
            "meanModulationBindingVisits") == Catch::Approx(3.0));
    REQUIRE((double) property(
            property(exported, "workload"),
            "meanSpectralTransferBindingVisits") == Catch::Approx(6.0));
    REQUIRE((int64) property(
            property(property(exported, "stages"), "voiceRendering"),
            "count") == 2);
}

TEST_CASE("Audio performance reset rejects samples from the old generation",
        "[cycle-v2][audio][performance]") {
    AudioPerformanceMetrics metrics;
    metrics.resetAndEnable();
    auto stale = beginSample(metrics);
    stale.callbackDurationMicroseconds = 100;

    metrics.resetAndEnable();
    metrics.publishRealtimeSample(stale);
    auto current = beginSample(metrics);
    current.callbackDurationMicroseconds = 200;
    metrics.publishRealtimeSample(current);
    metrics.serviceNonRealtime();

    const auto snapshot = metrics.snapshot();
    REQUIRE(snapshot.callbackDuration.count == 1);
    REQUIRE(snapshot.callbackDuration.totalMicroseconds == 200);
}

TEST_CASE("Audio performance handoff is bounded and drops telemetry only",
        "[cycle-v2][audio][performance]") {
    AudioPerformanceMetrics metrics;
    metrics.resetAndEnable();

    for (size_t index = 0; index < AudioPerformanceMetrics::queueCapacity; ++index) {
        auto sample = beginSample(metrics);
        sample.callbackDurationMicroseconds = 100;
        metrics.publishRealtimeSample(sample);
    }
    metrics.serviceNonRealtime();

    const auto snapshot = metrics.snapshot();
    REQUIRE(snapshot.callbackDuration.count
            == AudioPerformanceMetrics::queueCapacity - 1);
    REQUIRE(snapshot.telemetryDrops == 1);
}

TEST_CASE("Disabled audio performance collection does not start samples",
        "[cycle-v2][audio][performance]") {
    AudioPerformanceMetrics metrics;
    metrics.resetAndEnable();
    metrics.disable();
    AudioPerformanceMetrics::RealtimeSample sample;
    REQUIRE_FALSE(metrics.beginRealtimeSample(sample, 256, 48'000.0));
}
