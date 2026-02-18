#pragma once

#include <vector>

#include <Array/Buffer.h>

#include "../../Curve.h"
#include "../../Intercept.h"

class V2WaveformAdapter {
public:
    static bool adaptWaveformSamples(
        Buffer<float> samples,
        int pointsWritten,
        float xMinimum,
        float xMaximum,
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> slope,
        Buffer<float> diffX,
        int& zeroIndex,
        int& oneIndex,
        std::vector<Intercept>& icpts,
        std::vector<Curve>& curves,
        bool& unsampleable);
};
