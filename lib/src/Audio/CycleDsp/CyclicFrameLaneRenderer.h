#pragma once

#include <Array/Buffer.h>

namespace CycleDsp {

struct CyclicFrameCompositionRequest {
    Buffer<float> currentFrame;
    Buffer<float> previousFrame;
    Buffer<float> fadeIn;
    Buffer<float> fadeOut;
    float phaseCycles {};
    float nextFramePortion {};
    bool firstCycle {};
    bool phaseShiftEnabled {};
};

struct CyclicFrameCompositionWorkspace {
    Buffer<float> biasedFrame;
    Buffer<float> shiftedCurrentFrame;
    Buffer<float> shiftedPreviousFrame;
    Buffer<float> previousHalfFrame;
};

struct CyclicFrameLaneStateView {
    Buffer<float> lastLerpHalf;
};

class CyclicFrameLaneRenderer {
public:
    static Buffer<float> compose(
            const CyclicFrameCompositionRequest& request,
            CyclicFrameLaneStateView state,
            const CyclicFrameCompositionWorkspace& workspace);
};

}
