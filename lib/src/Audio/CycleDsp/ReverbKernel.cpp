#include "ReverbKernel.h"

#include <Algo/FFT.h>
#include <Array/ScopedAlloc.h>
#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleDsp {

namespace {

constexpr int kernelBlockSize = 256;

void createVolumeRamp(
        const ReverbKernelConfiguration& configuration,
        int index,
        int numBlocks,
        Buffer<float> ramp) {
    const int rampUpBlocks = jlimit(3, 20, (int) (configuration.roomSize * 0.03f * numBlocks));
    float rampStart {};
    float rampEnd {};

    if (index < rampUpBlocks) {
        rampStart = 0.25f * (float) index / (float) (rampUpBlocks - 1);
        rampEnd = 0.25f * (float) (index + 1) / (float) (rampUpBlocks - 1);
    } else {
        const float scale = MathConstants<float>::halfPi
                * ((float) (NumberUtils::log2i((unsigned) numBlocks) + 8) * 0.1f - 0.6f);
        rampStart = 0.25f / scale
                * std::sin(scale * std::exp(index * -4.5f / (float) numBlocks))
                * std::sqrt((float) (numBlocks - index) / (float) numBlocks);
        rampEnd = 0.25f / scale
                * std::sin(scale * std::exp((index + 1) * -4.5f / (float) numBlocks))
                * std::sqrt((float) (numBlocks - (index + 1)) / (float) numBlocks);
    }

    ramp.ramp(rampStart, (rampEnd - rampStart) / (float) ramp.size());

    const int silence = (int) (configuration.roomSize * 450.f) - index * ramp.size();
    if (silence > 0) {
        ramp.zero(std::min(ramp.size(), silence));
    }
}

}

void buildReverbKernel(
        const ReverbKernelConfiguration& configuration,
        Buffer<float> left,
        Buffer<float> right) {
    if (left.empty() || left.size() % kernelBlockSize != 0) {
        return;
    }

    Transform fft;
    fft.allocate(kernelBlockSize, Transform::DivFwdByN, true);

    ScopedAlloc<float> memory(kernelBlockSize * 8);
    Buffer<float> cumulativeRolloff = memory.place(kernelBlockSize);
    Buffer<float> noise = memory.place(kernelBlockSize);
    Buffer<float> rolloff = memory.place(kernelBlockSize);
    Buffer<float> volumeRamp = memory.place(kernelBlockSize);
    Buffer<float> filtered = memory.place(kernelBlockSize);
    Buffer<float> filteredA = memory.place(kernelBlockSize);
    Buffer<float> forwardRamp = memory.place(kernelBlockSize);
    Buffer<float> inverseRamp = memory.place(kernelBlockSize);

    const int highStart = std::max(2, (int) (configuration.highPass * 0.05f * kernelBlockSize));
    const float highLevelA = 1.f - configuration.damping * configuration.highPass * 1.5f;
    const float highLevelRamp = (1.f - highLevelA) / (float) highStart;

    rolloff.zero();
    rolloff.ramp(1.f, -configuration.damping / (float) kernelBlockSize);
    forwardRamp.section(0, highStart).ramp(highLevelA, highLevelRamp);
    rolloff.section(0, highStart).mul(forwardRamp);
    rolloff.mul(MathConstants<float>::halfPi).sin();

    forwardRamp.ramp();
    forwardRamp.copyTo(inverseRamp);
    inverseRamp.subCRev(1.f);

    unsigned seed = kernelBlockSize ^ 0xc0d3db4d;
    Buffer<float> channels[] = { left, right };
    const int numChannels = right.empty() ? 1 : 2;
    const int numBlocks = left.size() / kernelBlockSize;

    for (int channel = 0; channel < numChannels; ++channel) {
        if (channels[channel].size() != left.size()) {
            return;
        }

        cumulativeRolloff.set(1.f);
        for (int block = 0; block < numBlocks; ++block) {
            Buffer<float> section = channels[channel].section(
                    block * kernelBlockSize,
                    kernelBlockSize);
            createVolumeRamp(configuration, block, numBlocks, volumeRamp);

            noise.rand(seed);
            fft.forward(noise);
            fft.getMagnitudes().mul(cumulativeRolloff);
            cumulativeRolloff.mul(rolloff);

            fft.inverse(filteredA);
            fft.getMagnitudes().mul(rolloff);
            fft.inverse(filtered);

            VecOps::mul(filteredA, inverseRamp, section);
            section.addProduct(forwardRamp, filtered);
            section.mul(volumeRamp);

            seed ^= 616137 * seed;
        }
    }
}

}
