#include "V2SamplerStages.h"

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
        while (currentIndex < waveCount - 1 && phase >= wx[currentIndex + 1]) {
            ++currentIndex;
        }

        if (currentIndex >= waveCount - 1) {
            out[i] = wy[waveCount - 1];
        } else {
            out[i] = static_cast<float>((phase - wx[currentIndex]) * sl[currentIndex] + wy[currentIndex]);
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
