#pragma once

#include "../../Runtime/AudioProcessTypes.h"

#include <Algo/FFT.h>

#include <vector>

namespace CycleV2 {

class FftBlockwiseDsp {
public:
    void prepare(size_t frameCount);
    void resetState();
    void setHalfCycleCarryEnabled(bool shouldEnable);
    void forward(const AudioProcessBlock& input, AudioProcessBlock& magnitude, AudioProcessBlock& phase);
    void inverse(const AudioProcessBlock& magnitude, const AudioProcessBlock* phase, AudioProcessBlock& output);

private:
    Buffer<float> blockBuffer(const AudioProcessBlock& block, size_t size) const;
    Buffer<float> writableBlockBuffer(AudioProcessBlock& block, size_t size) const;
    void applyHalfCycleCarry(AudioProcessBlock& output);
    void allocateHalfCycleCarry();
    int binCount() const;

    bool hasCarry {};
    bool halfCycleCarryEnabled { true };
    size_t preparedFrameCount {};
    Transform transform;
    ScopedAlloc<float> carryMemory;
    Buffer<float> carryHalf;
    Buffer<float> rawHalf;
    Buffer<float> fadeIn;
    Buffer<float> fadeOut;
    std::vector<float> scratchMagnitude;
    std::vector<float> scratchPhase;
};

}
