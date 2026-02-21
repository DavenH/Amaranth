#pragma once

#include "../../../Array/Buffer.h"

namespace V2WaveSampling {
inline float sampleAtPhase(
    double phase,
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> slope,
    int wavePointCount,
    int& ioSampleIndex) noexcept {
    while (ioSampleIndex < wavePointCount - 2 && phase >= waveX[ioSampleIndex + 1]) {
        ++ioSampleIndex;
    }

    while (ioSampleIndex > 0 && phase < waveX[ioSampleIndex]) {
        --ioSampleIndex;
    }

    float sampled = 0.0f;
    if (phase <= waveX[0]) {
        sampled = waveY[0];
    } else if (phase >= waveX[wavePointCount - 1]) {
        sampled = waveY[wavePointCount - 1];
    } else {
        sampled = static_cast<float>((phase - waveX[ioSampleIndex]) * slope[ioSampleIndex] + waveY[ioSampleIndex]);
    }

    if (sampled != sampled) {
        return 0.0f;
    }

    return sampled;
}
}
