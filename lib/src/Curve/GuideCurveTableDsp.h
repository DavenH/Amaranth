#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "GuideCurveProvider.h"

class PreparedGuideCurveTable {
public:
    static constexpr int maximumResolutionRatio = 256;

    // Call off-thread again whenever the source table changes.
    void prepare(Buffer<float> table);
    bool copyTo(Buffer<float> destination) const;

private:
    int tableSize {};
    std::array<int, maximumResolutionRatio + 1> offsets {};
    std::vector<float> samples;
};

struct GuideCurveSamplingWork {
    uint64_t preparedCopies {};
    uint64_t downsampleOperations {};
};

struct GuideCurveTableParameters {
    float noiseLevel {};
    float verticalOffsetLevel {};
    float phaseOffsetLevel {};
    int seed {};
};

class GuideCurveTableDsp {
public:
    static void initializeNoise(Buffer<float> noise);
    static int stableSeed(int guideIndex);

    static float tableValue(
            Buffer<Float32> table,
            Buffer<float> noise,
            const GuideCurveTableParameters& parameters,
            float progress,
            const GuideCurveProvider::NoiseContext& context);
    static void sampleDownAddNoise(
            Buffer<Float32> table,
            Buffer<float> noise,
            Buffer<float> phaseScratch,
            const GuideCurveTableParameters& parameters,
            Buffer<float> destination,
            const GuideCurveProvider::NoiseContext& context,
            const PreparedGuideCurveTable* prepared = nullptr,
            GuideCurveSamplingWork* work = nullptr);
};
