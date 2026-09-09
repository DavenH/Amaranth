#include "Runtime/LegacyOutputRateAdapter.h"

namespace CycleV2 {

namespace {

size_t nextPowerOfTwo(size_t value) {
    size_t result = 1;
    while (result < value) {
        result <<= 1u;
    }
    return result;
}

}

void LegacyOutputRateAdapter::prepare(
        double outputSampleRate,
        int outputBlockSize,
        int channelCount) {
    blockAdapter.prepare(outputSampleRate);
    const int ringSize = (int) nextPowerOfTwo((size_t) outputBlockSize * 2);
    for (int channel = 0; channel < channelCount; ++channel) {
        hermiteMemory[(size_t) channel].resize((size_t) ringSize);
        hermiteStates[(size_t) channel].init({
                hermiteMemory[(size_t) channel].data(),
                ringSize
        });
        hermiteStates[(size_t) channel].reset(
                CycleDsp::InternalRateBlockAdapter::internalSampleRate
                / outputSampleRate);
        resampled[(size_t) channel].resize((size_t) outputBlockSize);
        accumulated[(size_t) channel].allocate(outputBlockSize * 2);
        accumulated[(size_t) channel].reset();
        accumulated[(size_t) channel].write(0.f);
    }
}

int LegacyOutputRateAdapter::convertBlockSize(int outputFrameCount) {
    return blockAdapter.convertBlockSize(outputFrameCount);
}

int LegacyOutputRateAdapter::convertSampleOffset(int outputSampleOffset) const {
    return blockAdapter.convertSampleOffset(outputSampleOffset);
}

bool LegacyOutputRateAdapter::convertAudio(
        const std::array<std::vector<float>, 2>& internal,
        int internalFrameCount,
        float* const* outputChannels,
        int outputChannelCount,
        int outputFrameCount) {
    for (int channel = 0; channel < outputChannelCount; ++channel) {
        if (internalFrameCount > 0) {
            const int resampledSize = hermiteStates[(size_t) channel].resample(
                    {
                            const_cast<float*>(internal[(size_t) channel].data()),
                            internalFrameCount
                    },
                    {
                            resampled[(size_t) channel].data(),
                            (int) resampled[(size_t) channel].size()
                    });
            accumulated[(size_t) channel].write({
                    resampled[(size_t) channel].data(),
                    resampledSize
            });
        }
        if (!accumulated[(size_t) channel].hasDataFor(outputFrameCount)) {
            return false;
        }
        accumulated[(size_t) channel].read(outputFrameCount).copyTo({
                outputChannels[channel],
                outputFrameCount
        });
    }
    return true;
}

}
