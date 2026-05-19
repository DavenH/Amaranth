#pragma once

#include "../../Runtime/NodeAudioProcessor.h"

#include <Algo/FFT.h>

namespace CycleV2 {

class FftBlockwiseDsp {
public:
    void prepare(size_t frameCount);
    void forward(const AudioProcessBlock& input, AudioProcessBlock& magnitude, AudioProcessBlock& phase);
    void inverse(const AudioProcessBlock& magnitude, const AudioProcessBlock* phase, AudioProcessBlock& output);

private:
    Buffer<float> blockBuffer(const AudioProcessBlock& block, size_t size) const;
    Buffer<float> writableBlockBuffer(AudioProcessBlock& block, size_t size) const;
    int binCount() const;

    size_t preparedFrameCount {};
    Transform transform;
};

}
