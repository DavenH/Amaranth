#include "PitchKernelBuilder.h"

#include "PitchKernelPrimes.h"

namespace {
    constexpr float peakWidth = 0.25f;
    constexpr float troughStart = 0.25f;
    constexpr float troughEnd = 0.75f;
}

void PitchKernelBuilder::paintPrimeHarmonics(
        Buffer<float> kernel,
        Buffer<float> harmonicRatios,
        Buffer<float> harmonicDistance,
        int numHarmonics) {
    int peakStart = 0;
    int troughStartIndex = 0;

    for (int prime: pitchKernelPrimes) {
        if (prime >= numHarmonics) {
            break;
        }

        harmonicRatios.copyTo(harmonicDistance);
        harmonicDistance.sub((float) prime).abs();

        paintPeakBand(kernel, harmonicDistance, peakStart);
        paintTroughBands(kernel, harmonicRatios, harmonicDistance, troughStartIndex);
    }
}

void PitchKernelBuilder::paintPeakBand(
        Buffer<float> kernel,
        const Buffer<float>& harmonicDistance,
        int& startIndex) {
    const float twoPi = MathConstants<float>::twoPi;

    for (int k = startIndex; k < harmonicDistance.size(); ++k) {
        if (harmonicDistance[k] < peakWidth) {
            startIndex = k;

            while (k < harmonicDistance.size() && harmonicDistance[k] < peakWidth) {
                kernel[k] = cosf(twoPi * harmonicDistance[k]);
                ++k;
            }
            break;
        }
    }
}

void PitchKernelBuilder::paintTroughBands(
        Buffer<float> kernel,
        const Buffer<float>& harmonicRatios,
        const Buffer<float>& harmonicDistance,
        int& startIndex) {
    const float twoPi = MathConstants<float>::twoPi;
    bool firstTrough = true;

    for (int k = startIndex; k < harmonicDistance.size(); ++k) {
        if (harmonicDistance[k] >= troughStart && harmonicDistance[k] < troughEnd) {
            startIndex = k;

            while (k < harmonicDistance.size()
                    && harmonicDistance[k] >= troughStart
                    && harmonicDistance[k] < troughEnd) {
                kernel[k] += cosf(twoPi * harmonicRatios[k]) / 2;
                ++k;
            }

            if (!firstTrough) {
                break;
            }

            firstTrough = false;
        }
    }
}
