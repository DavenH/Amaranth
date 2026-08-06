#pragma once

#include "GuideCurveProvider.h"

#include <cstdint>

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
            const GuideCurveProvider::NoiseContext& context);
};
