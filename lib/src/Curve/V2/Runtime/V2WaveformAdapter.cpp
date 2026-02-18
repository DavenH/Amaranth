#include <Array/VecOps.h>

#include "V2WaveformAdapter.h"

bool V2WaveformAdapter::adaptWaveformSamples(
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
    bool& unsampleable) {
    if (pointsWritten <= 1 || samples.size() < pointsWritten) {
        return false;
    }

    Buffer<float> waveXUsed = waveX.withSize(pointsWritten);
    Buffer<float> waveYUsed = waveY.withSize(pointsWritten);
    samples.withSize(pointsWritten).copyTo(waveYUsed);

    float dx = (xMaximum - xMinimum) / static_cast<float>(jmax(1, pointsWritten - 1));
    waveXUsed.ramp(xMinimum, dx);

    Buffer<float> slopeUsed = slope.withSize(pointsWritten - 1);
    Buffer<float> diffUsed = diffX.withSize(pointsWritten - 1);

    VecOps::sub(waveXUsed + 1, waveXUsed, diffUsed);
    VecOps::sub(waveYUsed + 1, waveYUsed, slopeUsed);
    diffUsed.threshLT(1e-6f);
    slopeUsed.div(diffUsed);

    zeroIndex = 0;
    oneIndex = jmax(0, pointsWritten - 2);

    for (int i = 0; i < pointsWritten - 1; ++i) {
        if (waveX[i] <= 0.0f && waveX[i + 1] > 0.0f) {
            zeroIndex = i;
            break;
        }
    }

    for (int i = 0; i < pointsWritten - 1; ++i) {
        if (waveX[i] < 1.0f && waveX[i + 1] >= 1.0f) {
            oneIndex = i;
            break;
        }
    }

    icpts.clear();
    curves.clear();
    unsampleable = false;
    return true;
}
