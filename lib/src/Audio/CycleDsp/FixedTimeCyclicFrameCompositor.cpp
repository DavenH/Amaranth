#include "FixedTimeCyclicFrameCompositor.h"

#include <Algo/Resampling.h>

namespace CycleDsp {

bool FixedTimeFrameClock::prepare(int intervalSamplesToUse) {
    if (intervalSamplesToUse <= 0) {
        return false;
    }

    interval = intervalSamplesToUse;
    reset();
    return true;
}

void FixedTimeFrameClock::reset() {
    frontier = (uint64_t) interval;
}

bool FixedTimeFrameClock::isFrontier(uint64_t voiceSample) const {
    return interval > 0 && voiceSample == frontier;
}

void FixedTimeFrameClock::advance() {
    frontier += (uint64_t) interval;
}

bool FixedTimeCyclicFrameCompositor::makeRaisedCosineWeights(
        int intervalSamples,
        Buffer<float> weights) {
    if (intervalSamples <= 0 || weights.size() < intervalSamples + 1) {
        return false;
    }

    auto transition = weights.withSize(intervalSamples + 1);
    const float pi = MathConstants<float>::pi;
    transition.ramp(-0.5f * pi, pi / (float) intervalSamples);
    transition.sin().add(1.f).mul(0.5f);
    transition.front() = 0.f;
    transition.back() = 1.f;
    return true;
}

float FixedTimeCyclicFrameCompositor::samplePeriodicFrame(
        Buffer<float> frame,
        double phaseCycles) {
    const int frameSize = frame.size();
    if (frameSize <= 2 || (frameSize & (frameSize - 1)) != 0) {
        return 0.f;
    }

    const int mask = frameSize - 1;
    const double position = phaseCycles * frameSize;
    const int index = (int) position;
    const float fraction = (float) (position - index);
    return Resampling::hermite6_3(
            fraction,
            frame[(index - 2) & mask],
            frame[(index - 1) & mask],
            frame[index & mask],
            frame[(index + 1) & mask],
            frame[(index + 2) & mask],
            frame[(index + 3) & mask]);
}

float FixedTimeCyclicFrameCompositor::compose(
        const FixedTimeCyclicFrameSampleRequest& request) {
    if (request.previousFrame.size() != request.targetFrame.size()) {
        return 0.f;
    }

    const float weight = jlimit(0.f, 1.f, request.targetWeight);
    const float previous = samplePeriodicFrame(
            request.previousFrame,
            request.phaseCycles);
    if (weight == 0.f) {
        return previous;
    }
    const float target = samplePeriodicFrame(
            request.targetFrame,
            request.phaseCycles);
    return previous + weight * (target - previous);
}

}
