#include "Runtime/SpectralFrameTransformStage.h"

#include <Audio/CycleDsp/SpectralLayerCore.h>

namespace CycleV2 {

namespace {

bool isPowerOfTwo(int value) {
    return value > 1 && (value & (value - 1)) == 0;
}

}

bool SpectralFrameTransformStage::supportsFrameSize(int frameSize) {
    return isPowerOfTwo(frameSize);
}

bool SpectralFrameTransformStage::prepare(int maximumFrameSize) {
    if (!supportsFrameSize(maximumFrameSize)) {
        return false;
    }

    transforms.clear();
    for (int frameSize = 2; frameSize <= maximumFrameSize; frameSize *= 2) {
        auto transform = std::make_unique<Transform>();
        transform->allocate(frameSize, Transform::DivFwdByN, true);
        transform->setRemovesOffset(true);
        transform->setExclusiveRealtimeAccess(true);
        transforms.push_back(std::move(transform));
    }
    return true;
}

Transform* SpectralFrameTransformStage::transformFor(int frameSize) {
    if (!isPowerOfTwo(frameSize)) {
        return nullptr;
    }
    int index = 0;
    for (int size = 2; size < frameSize; size *= 2) {
        ++index;
    }
    return index < (int) transforms.size()
            ? transforms[(size_t) index].get()
            : nullptr;
}

void SpectralFrameTransformStage::forward(
        Transform& transform,
        Buffer<float> timeFrame,
        Buffer<float> magnitude,
        Buffer<float> phase,
        int activeHarmonicCount,
        const CycleDsp::SpectralFrameCapture& capture,
        int channel) const {
    capture.capture(
            CycleDsp::SpectralStage::TimeFrame,
            channel,
            timeFrame);
    transform.forward(timeFrame);
    transform.copyFullPolarSpectrumTo(magnitude, phase);
    capture.capture(
            CycleDsp::SpectralStage::ForwardFft,
            channel,
            magnitude.section(1, activeHarmonicCount),
            phase.section(1, activeHarmonicCount));
}

void SpectralFrameTransformStage::inverse(
        Transform& transform,
        Buffer<float> magnitude,
        Buffer<float> phase,
        Buffer<float> output,
        int activeHarmonicCount,
        bool clearInactiveBins,
        const CycleDsp::SpectralFrameCapture& capture,
        int channel) const {
    capture.capture(
            CycleDsp::SpectralStage::PostLayerSpectrum,
            channel,
            magnitude.section(1, activeHarmonicCount),
            phase.section(1, activeHarmonicCount));
    if (clearInactiveBins) {
        CycleDsp::SpectralLayerCore::clearBinsAbove(
                magnitude,
                phase,
                activeHarmonicCount + 1);
    }
    transform.setFullPolarSpectrum(magnitude, phase);
    transform.inverse(output);
    capture.capture(
            CycleDsp::SpectralStage::ReconstructedFrame,
            channel,
            output);
}

}
