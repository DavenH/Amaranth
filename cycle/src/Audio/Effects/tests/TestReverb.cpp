#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <Array/Buffer.h>

#include "../Reverb.h"

TEST_CASE("Cycle reverb renders a Dirac impulse as a stereo tail",
        "[cycle][audio][reverb]") {
    constexpr int blockSize = 512;
    constexpr int kernelSize = 4096;
    constexpr int renderedBlocks = 10;

    ScopedJuceInitialiser_GUI juce;
    ReverbEffect reverb(nullptr);
    reverb.setPendingAction(ReverbEffect::blockSize, blockSize);
    reverb.audioThreadUpdate();
    reverb.paramChanged(ReverbEffect::Width, 1.0, false);
    reverb.paramChanged(ReverbEffect::Wet, 1.0, false);
    reverb.createKernel(kernelSize);

    AudioSampleBuffer audio(2, blockSize);
    double tailMagnitude {};
    double lateMagnitude {};
    double stereoDifference {};
    int nonSilentTailBlocks {};
    bool outputIsFinite = true;

    for (int block = 0; block < renderedBlocks; ++block) {
        audio.clear();
        if (block == 0) {
            audio.setSample(0, 0, 1.f);
            audio.setSample(1, 0, 1.f);
        }

        reverb.processBuffer(audio);

        Buffer<float> left(audio, 0);
        Buffer<float> right(audio, 1);
        const double blockMagnitude = left.normL1() + right.normL1();
        if (block > 0 && blockMagnitude > 1.0e-5) {
            ++nonSilentTailBlocks;
        }
        if (block > 0) {
            tailMagnitude += blockMagnitude;
        }
        if (block >= renderedBlocks / 2) {
            lateMagnitude += blockMagnitude;
        }

        for (int sample = 0; sample < blockSize; ++sample) {
            outputIsFinite = outputIsFinite
                    && std::isfinite(left[sample])
                    && std::isfinite(right[sample]);
            stereoDifference += std::abs(left[sample] - right[sample]);
        }
    }

    REQUIRE(outputIsFinite);
    REQUIRE(tailMagnitude > 0.01);
    REQUIRE(lateMagnitude > 0.001);
    REQUIRE(nonSilentTailBlocks >= 4);
    REQUIRE(stereoDifference > 0.001);
}

TEST_CASE("Cycle reverb settles pending preset parameters deterministically",
        "[cycle][audio][reverb][parity]") {
    constexpr int blockSize = 512;
    constexpr int renderedBlocks = 4;

    ScopedJuceInitialiser_GUI juce;
    ReverbEffect reverb(nullptr);
    reverb.setPendingAction(ReverbEffect::blockSize, blockSize);
    reverb.paramChanged(ReverbEffect::Size, 0.2, false);
    reverb.paramChanged(ReverbEffect::Damp, 0.2, false);
    reverb.paramChanged(ReverbEffect::Width, 0.0, false);
    reverb.paramChanged(ReverbEffect::Highpass, 0.616, false);
    reverb.paramChanged(ReverbEffect::Wet, 0.22, false);

    const auto render = [&]() {
        reverb.updateParametersToTarget();
        std::vector<float> result;
        result.reserve(2 * blockSize * renderedBlocks);
        AudioSampleBuffer audio(2, blockSize);
        for (int block = 0; block < renderedBlocks; ++block) {
            audio.clear();
            if (block == 0) {
                audio.setSample(0, 0, 1.f);
                audio.setSample(1, 0, 1.f);
            }
            reverb.processBuffer(audio);
            for (int channel = 0; channel < 2; ++channel) {
                const Buffer<float> samples(audio, channel);
                result.insert(
                        result.end(),
                        samples.get(),
                        samples.get() + samples.size());
            }
        }
        return result;
    };

    const auto first = render();
    const auto second = render();

    REQUIRE(first == second);
}
