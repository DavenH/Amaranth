#pragma once

#include <Array/Buffer.h>

#include <cstdint>

namespace CycleDsp {

class FixedTimeFrameClock {
public:
    bool prepare(int intervalSamplesToUse);
    void reset();
    bool isFrontier(uint64_t voiceSample) const;
    void advance();

    int intervalSamples() const { return interval; }
    uint64_t nextFrontier() const { return frontier; }

private:
    int interval {};
    uint64_t frontier {};
};

struct FixedTimeCyclicFrameSampleRequest {
    Buffer<float> previousFrame;
    Buffer<float> targetFrame;
    double phaseCycles {};
    float targetWeight {};
};

class FixedTimeCyclicFrameCompositor {
public:
    static bool makeRaisedCosineWeights(
            int intervalSamples,
            Buffer<float> weights);
    static float samplePeriodicFrame(
            Buffer<float> frame,
            double phaseCycles);
    static float compose(const FixedTimeCyclicFrameSampleRequest& request);
};

}
