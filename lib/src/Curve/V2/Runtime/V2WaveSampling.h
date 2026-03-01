#pragma once

#include <vector>

#include <Array/Buffer.h>
#include <Curve/IDeformer.h>
#include <Util/NumberUtils.h>
#include <Curve/V2/Runtime/V2RenderTypes.h>

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

inline float sampleAtPhaseDecoupled(
    double phase,
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> slope,
    int wavePointCount,
    int& ioSampleIndex,
    IDeformer* path,
    const std::vector<V2DeformRegion>* deformRegions,
    int noiseSeed,
    int phaseOffsetSeed,
    int vertOffsetSeed) noexcept {
    float sampled = sampleAtPhase(phase, waveX, waveY, slope, wavePointCount, ioSampleIndex);
    if (path == nullptr || deformRegions == nullptr || deformRegions->empty()) {
        return sampled;
    }

    for (const auto& region : *deformRegions) {
        if (! NumberUtils::within<float>(static_cast<float>(phase), region.start.x, region.end.x)) {
            continue;
        }

        float diff = region.end.x - region.start.x;
        if (diff <= 0.0f) {
            continue;
        }

        IDeformer::NoiseContext noise;
        noise.noiseSeed = noiseSeed;
        noise.phaseOffset = phaseOffsetSeed;
        noise.vertOffset = vertOffsetSeed;

        float progress = (static_cast<float>(phase) - region.start.x) / diff;
        return sampled + region.amplitude * path->getTableValue(region.deformChan, progress, noise);
    }

    return sampled;
}
}
