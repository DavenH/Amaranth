#include "Runtime/PerformanceDistribution.h"

#include <algorithm>
#include <cmath>

namespace CycleV2 {

using namespace juce;

namespace {

constexpr std::array<uint64_t, PerformanceDistribution::bucketCount - 1>
        bucketUpperMicroseconds {
            50, 100, 250, 500, 1000, 2000, 4000, 8000, 16000, 33000, 66000, 100000
        };

size_t bucketFor(uint64_t microseconds) {
    const auto found = std::lower_bound(
            bucketUpperMicroseconds.begin(),
            bucketUpperMicroseconds.end(),
            microseconds);
    return (size_t) std::distance(bucketUpperMicroseconds.begin(), found);
}

}

void recordPerformanceSample(
        PerformanceDistribution& distribution,
        uint64_t microseconds) {
    ++distribution.count;
    distribution.totalMicroseconds += microseconds;
    distribution.maximumMicroseconds = std::max(
            distribution.maximumMicroseconds,
            microseconds);
    ++distribution.buckets[bucketFor(microseconds)];
}

double performancePercentileMilliseconds(
        const PerformanceDistribution& distribution,
        double percentile) {
    if (distribution.count == 0) {
        return 0.0;
    }

    const uint64_t target = std::max<uint64_t>(
            1,
            (uint64_t) std::ceil((double) distribution.count * percentile));
    uint64_t cumulative {};
    for (size_t index = 0; index < distribution.buckets.size(); ++index) {
        cumulative += distribution.buckets[index];
        if (cumulative >= target) {
            const uint64_t upper = index < bucketUpperMicroseconds.size()
                    ? bucketUpperMicroseconds[index]
                    : distribution.maximumMicroseconds;
            return (double) upper / 1000.0;
        }
    }
    return (double) distribution.maximumMicroseconds / 1000.0;
}

var performanceDistributionToVar(
        const PerformanceDistribution& distribution) {
    auto* object = new DynamicObject();
    object->setProperty("count", (int64) distribution.count);
    object->setProperty(
            "meanMs",
            distribution.count == 0
                    ? 0.0
                    : (double) distribution.totalMicroseconds
                            / (double) distribution.count / 1000.0);
    object->setProperty(
            "p50Ms",
            performancePercentileMilliseconds(distribution, 0.50));
    object->setProperty(
            "p95Ms",
            performancePercentileMilliseconds(distribution, 0.95));
    object->setProperty(
            "p99Ms",
            performancePercentileMilliseconds(distribution, 0.99));
    object->setProperty(
            "maxMs",
            (double) distribution.maximumMicroseconds / 1000.0);
    return var(object);
}

}
