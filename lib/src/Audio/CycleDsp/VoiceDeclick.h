#pragma once

#include <Array/Buffer.h>

namespace CycleDsp {

class VoiceDeclick {
public:
    static constexpr double legacySampleRate = 44'100.;
    static constexpr double attackDurationSeconds = 0.00145;
    static constexpr double releaseDurationSeconds = 0.01;

    static int attackSampleCount(double sampleRate);
    static int releaseSampleCount(double sampleRate);

    static void prepareAttack(Buffer<float> envelope);
    static void prepareRelease(Buffer<float> envelope);
    static void applyReleaseTail(
            Buffer<float> envelope,
            Buffer<float> releaseEnvelope,
            int releaseSamplesRemaining);
};

}
