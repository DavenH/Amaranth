#pragma once

#include <memory>
#include <vector>

#include <Algo/FFT.h>
#include <Array/Buffer.h>
#include <Audio/CycleDsp/SpectralStageCapture.h>

namespace CycleV2 {

class SpectralFrameTransformStage final {
public:
    static bool supportsFrameSize(int frameSize);
    bool prepare(int maximumFrameSize);
    Transform* transformFor(int frameSize);

    void forward(
            Transform& transform,
            Buffer<float> timeFrame,
            Buffer<float> magnitude,
            Buffer<float> phase,
            int activeHarmonicCount,
            const CycleDsp::SpectralFrameCapture& capture,
            int channel) const;
    void inverse(
            Transform& transform,
            Buffer<float> magnitude,
            Buffer<float> phase,
            Buffer<float> output,
            int activeHarmonicCount,
            bool clearInactiveBins,
            const CycleDsp::SpectralFrameCapture& capture,
            int channel) const;

private:
    std::vector<std::unique_ptr<Transform>> transforms;
};

}
