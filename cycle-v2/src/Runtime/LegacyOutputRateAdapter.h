#pragma once

#include <array>
#include <vector>

#include <Algo/HermiteResampler.h>
#include <Array/RingBuffer.h>
#include <Audio/CycleDsp/InternalRateBlockAdapter.h>

namespace CycleV2 {

class LegacyOutputRateAdapter {
public:
    void prepare(double outputSampleRate, int outputBlockSize, int channelCount);
    int convertBlockSize(int outputFrameCount);
    int convertSampleOffset(int outputSampleOffset) const;
    bool convertAudio(
            const std::array<std::vector<float>, 2>& internal,
            int internalFrameCount,
            float* const* outputChannels,
            int outputChannelCount,
            int outputFrameCount);

private:
    CycleDsp::InternalRateBlockAdapter blockAdapter;
    std::array<HermiteState, 2> hermiteStates;
    std::array<std::vector<float>, 2> hermiteMemory;
    std::array<std::vector<float>, 2> resampled;
    std::array<ReadWriteBuffer, 2> accumulated;
};

}
