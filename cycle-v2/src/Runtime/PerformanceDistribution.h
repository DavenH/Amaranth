#pragma once

#include <JuceHeader.h>

#include <array>
#include <cstdint>

namespace CycleV2 {

struct PerformanceDistribution {
    static constexpr size_t bucketCount = 13;

    uint64_t count {};
    uint64_t totalMicroseconds {};
    uint64_t maximumMicroseconds {};
    std::array<uint64_t, bucketCount> buckets {};
};

void recordPerformanceSample(
        PerformanceDistribution& distribution,
        uint64_t microseconds);
double performancePercentileMilliseconds(
        const PerformanceDistribution& distribution,
        double percentile);
juce::var performanceDistributionToVar(
        const PerformanceDistribution& distribution);

}
