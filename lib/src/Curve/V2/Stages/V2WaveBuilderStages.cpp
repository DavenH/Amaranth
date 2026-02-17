#include "V2WaveBuilderStages.h"

#include <cmath>

#include "../../../Array/ScopedAlloc.h"
#include "../../../Array/VecOps.h"

namespace {
Buffer<float> getTransferTable() {
    static ScopedAlloc<float> transferMemory;
    static Buffer<float> transferTable;
    static bool initialized = false;

    if (! initialized) {
        const float pi = MathConstants<float>::pi;

        transferMemory.ensureSize(Curve::resolution);
        transferTable = transferMemory.withSize(Curve::resolution);

        double isize = 1.0 / static_cast<double>(Curve::resolution);
        for (int i = 0; i < Curve::resolution; ++i) {
            double x = i * isize;
            transferTable[i] = static_cast<float>(
                x
                - 0.2180285 * std::sin(2.0 * pi * x)
                + 0.0322599 * std::sin(4.0 * pi * x)
                - 0.0018794 * std::sin(6.0 * pi * x));
        }

        initialized = true;
    }

    return transferTable;
}
}

bool V2DefaultWaveBuilderStage::run(
    const std::vector<Curve>& curves,
    int curveCount,
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> diffX,
    Buffer<float> slope,
    int& outWavePointCount,
    int& zeroIndex,
    int& oneIndex,
    const V2WaveBuilderContext& context) noexcept {
    outWavePointCount = 0;
    zeroIndex = 0;
    oneIndex = 0;

    if (curveCount < 2 || curveCount > static_cast<int>(curves.size())) {
        return false;
    }

    const Buffer<float> transferTable = getTransferTable();

    int totalRes = 0;
    int halfRes = Curve::resolution / 2;
    for (int i = 0; i < curveCount - 1; ++i) {
        int thisRes = halfRes >> curves[i].resIndex;
        int nextRes = halfRes >> curves[i + 1].resIndex;
        totalRes += jmin(thisRes, nextRes);
    }

    if (totalRes <= 1 || totalRes > waveX.size() || totalRes > waveY.size()) {
        return false;
    }

    waveX.withSize(totalRes).zero();
    waveY.withSize(totalRes).zero();

    int waveIdx = 0;
    for (int c = 0; c < curveCount - 1; ++c) {
        const Curve& thisCurve = curves[c];
        const Curve& nextCurve = curves[c + 1];

        int thisRes = halfRes >> thisCurve.resIndex;
        int nextRes = halfRes >> nextCurve.resIndex;
        int curveRes = jmin(thisRes, nextRes);
        int offset = halfRes >> thisCurve.resIndex;
        int xferInc = Curve::resolution / curveRes;

        int thisShift = jmax(0, nextCurve.resIndex - thisCurve.resIndex);
        int nextShift = jmax(0, thisCurve.resIndex - nextCurve.resIndex);

        for (int i = 0; i < curveRes; ++i) {
            float xfer = transferTable[i * xferInc];
            int indexA = (i << thisShift) + offset;
            int indexB = (i << nextShift);

            float t1x = thisCurve.transformX[indexA] * (1.0f - xfer);
            float t1y = thisCurve.transformY[indexA] * (1.0f - xfer);
            float t2x = nextCurve.transformX[indexB] * xfer;
            float t2y = nextCurve.transformY[indexB] * xfer;

            waveX[waveIdx] = t1x + t2x;
            waveY[waveIdx] = t1y + t2y;
            ++waveIdx;
        }
    }

    outWavePointCount = waveIdx;
    if (outWavePointCount <= 1) {
        return false;
    }

    zeroIndex = 0;
    oneIndex = outWavePointCount - 2;
    for (int i = 0; i < outWavePointCount - 1; ++i) {
        if (waveX[i] <= 0.0f && waveX[i + 1] > 0.0f) {
            zeroIndex = i;
            break;
        }
    }

    for (int i = 0; i < outWavePointCount - 1; ++i) {
        if (waveX[i] < 1.0f && waveX[i + 1] >= 1.0f) {
            oneIndex = i;
            break;
        }
    }

    Buffer<float> slopeUsed = slope.withSize(outWavePointCount - 1);
    Buffer<float> diffUsed = diffX.withSize(outWavePointCount - 1);

    VecOps::sub(waveX + 1, waveX, diffUsed);
    VecOps::sub(waveY + 1, waveY, slopeUsed);
    diffUsed.threshLT(1e-6f);
    slopeUsed.div(diffUsed);

    return true;
}
