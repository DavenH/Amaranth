#pragma once

#include <vector>

#include <Array/Buffer.h>
#include <Curve/IDeformer.h>
#include <Util/NumberUtils.h>
#include <Curve/V2/Runtime/V2PhasePolicies.h>
#include <Curve/V2/Runtime/V2RenderTypes.h>

namespace V2WaveSampling {
inline float sampleAtPhase(
        double phase,
        const V2WaveBuffers& waveBuffers,
        int wavePointCount,
        int& ioSampleIndex) noexcept {
    Buffer<float> waveX = waveBuffers.waveX;
    Buffer<float> waveY = waveBuffers.waveY;
    Buffer<float> slope = waveBuffers.slope;

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
        const V2WaveBuffers& waveBuffers,
        int wavePointCount,
        int& ioSampleIndex,
        const V2DecoupledDeformContext& deformContext) noexcept {
    float sampled = sampleAtPhase(phase, waveBuffers, wavePointCount, ioSampleIndex);
    if (! deformContext.isEnabled()) {
        return sampled;
    }

    for (const auto& region : *deformContext.deformRegions) {
        if (! NumberUtils::within<float>(static_cast<float>(phase), region.start.x, region.end.x)) {
            continue;
        }

        float diff = region.end.x - region.start.x;
        if (diff <= 0.0f) {
            continue;
        }

        IDeformer::NoiseContext noise;
        noise.noiseSeed = deformContext.noiseSeed;
        noise.phaseOffset = deformContext.phaseOffsetSeed;
        noise.vertOffset = deformContext.vertOffsetSeed;

        float progress = (static_cast<float>(phase) - region.start.x) / diff;
        return sampled + region.amplitude * deformContext.path->getTableValue(region.deformChan, progress, noise);
    }

    return sampled;
}

template<typename PhasePolicy>
inline V2RenderResult sampleBlock(
        PhasePolicy& policy,
        const V2WaveBuffers& waveBuffers,
        int wavePointCount,
        Buffer<float> output,
        int& ioSampleIndex,
        const V2DecoupledDeformContext& deform = {}) noexcept {
    V2RenderResult result;

    if (output.empty() || wavePointCount <= 1) {
        return result;
    }

    int currentIndex = jlimit(0, wavePointCount - 2, ioSampleIndex);
    bool hasDeform = deform.isEnabled();

    for (int i = 0; i < output.size(); ++i) {
        double phase = policy.currentPhase();

        if (hasDeform) {
            output[i] = sampleAtPhaseDecoupled(phase, waveBuffers, wavePointCount, currentIndex, deform);
        } else {
            output[i] = sampleAtPhase(phase, waveBuffers, wavePointCount, currentIndex);
        }

        if (output[i] != output[i]) {
            output[i] = 0.0f;
        }

        policy.advance();
    }

    policy.finalize();
    ioSampleIndex = currentIndex;

    result.rendered = true;
    result.stillActive = true;
    result.samplesWritten = output.size();
    return result;
}
}
