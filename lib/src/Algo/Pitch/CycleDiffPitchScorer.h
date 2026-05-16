#pragma once

#include <limits>
#include <vector>

#include "../../Array/Buffer.h"
#include "PitchTrackingRequest.h"

using std::vector;

class CycleDiffPitchScorer {
public:
    static void precomputePeriods(
        const PitchTrackingRequest& request,
        double frequencyOfA4,
        int sampleRate,
        vector<int>& periods);

    static float scorePeriod(Buffer<float> signal, int period, Buffer<float> scratch);

    static void scorePeriods(
        Buffer<float> signal,
        const vector<int>& periods,
        Buffer<float> scores,
        Buffer<float> scratch);

    static void preferHigherOctaves(Buffer<float> scores, float octaveTolerance = 1.08f);

    static int findBestScore(Buffer<float> scores, int initialIndex, float& bestScore);
};
