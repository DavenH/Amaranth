#pragma once

#include <vector>

#include <Array/Buffer.h>
#include <Curve/GuideCurveProvider.h>
#include <Util/NumberUtils.h>
#include <Curve/V2/Sampling/V2PhasePolicies.h>
#include <Curve/V2/Core/V2RenderTypes.h>

namespace V2WaveSampling {
inline float sampleAtPhase(
        double phase,
        const V2WaveBuffers& waveBuffers,
        int wavePointCount,
        int& ioSampleIndex) noexcept {
    Buffer<float> waveX = waveBuffers.waveX;
    Buffer<float> waveY = waveBuffers.waveY;
    Buffer<float> slope = waveBuffers.slope;

    if (wavePointCount <= 1 || waveX.empty() || waveY.empty()) {
        return 0.0f;
    }

    if (ioSampleIndex >= wavePointCount) {
        ioSampleIndex = 0;
    }

    float sampled = 0.0f;
    if (phase >= waveX[wavePointCount - 1] || phase < waveX[0]) {
        sampled = static_cast<float>(phase);
    } else {
        if (ioSampleIndex > 0) {
            while (phase < waveX[ioSampleIndex - 1]) {
                --ioSampleIndex;
                if (ioSampleIndex == 0) {
                    ioSampleIndex = wavePointCount - 1;
                }
            }
        }

        while (phase >= waveX[ioSampleIndex]) {
            ++ioSampleIndex;
            if (ioSampleIndex >= wavePointCount) {
                ioSampleIndex = 0;
            }
        }

        if (ioSampleIndex == 0) {
            sampled = waveY[0];
        } else {
            sampled = static_cast<float>(
                (phase - waveX[ioSampleIndex - 1]) * slope[ioSampleIndex - 1]
                + waveY[ioSampleIndex - 1]);
        }
    }

    if (phase <= waveX[0]) {
        sampled = waveY[0];
    } else if (phase >= waveX[wavePointCount - 1]) {
        sampled = waveY[wavePointCount - 1];
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

        GuideCurveProvider::NoiseContext noise;
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
