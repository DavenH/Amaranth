#pragma once

#include "../../Array/Buffer.h"

class PitchKernelBuilder {
public:
    static void paintPrimeHarmonics(
        Buffer<float> kernel,
        Buffer<float> harmonicRatios,
        Buffer<float> harmonicDistance,
        int numHarmonics);

private:
    static void paintPeakBand(
        Buffer<float> kernel,
        const Buffer<float>& harmonicDistance,
        int& startIndex);

    static void paintTroughBands(
        Buffer<float> kernel,
        const Buffer<float>& harmonicRatios,
        const Buffer<float>& harmonicDistance,
        int& startIndex);
};
