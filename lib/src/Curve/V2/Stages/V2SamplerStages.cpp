#include "V2SamplerStages.h"
#include "../Runtime/V2WaveSampling.h"

V2RenderResult V2LinearSamplerStage::run(
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> slope,
    Buffer<float> output,
    const V2SamplerContext& context) noexcept {
    V2RenderResult result;

    int requested = context.request.numSamples;
    int waveCount = context.wavePointCount;

    if (requested <= 0 || output.empty() || waveCount <= 1) {
        return result;
    }

    int samples = jmin(requested, output.size());
    Buffer<float> out = output.withSize(samples);

    Buffer<float> wx = waveX.withSize(waveCount);
    Buffer<float> wy = waveY.withSize(waveCount);
    Buffer<float> sl = slope.withSize(waveCount - 1);

    if (wx.empty()) {
        out.zero();
        return result;
    }

    double phase = 0.0;
    double delta = context.request.deltaX;
    if (delta <= 0.0) {
        out.zero();
        return result;
    }

    int currentIndex = jlimit(0, waveCount - 2, context.zeroIndex);

    for (int i = 0; i < samples; ++i) {
        if (context.decoupledPath != nullptr && context.deformRegions != nullptr) {
            out[i] = V2WaveSampling::sampleAtPhaseDecoupled(
                phase,
                wx,
                wy,
                sl,
                waveCount,
                currentIndex,
                context.decoupledPath,
                context.deformRegions,
                0,
                context.phaseOffsetSeed,
                context.vertOffsetSeed);
        } else {
            out[i] = V2WaveSampling::sampleAtPhase(phase, wx, wy, sl, waveCount, currentIndex);
        }

        if (out[i] != out[i]) {
            out[i] = 0.0f;
        }

        phase += delta;
    }

    result.rendered = true;
    result.stillActive = true;
    result.samplesWritten = samples;
    return result;
}
