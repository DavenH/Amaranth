#include "SimpleCurveSampler.h"
#include "../../Array/VecOps.h"

#include "JuceHeader.h"

#include <climits>

float SimpleCurveSampler::transferTable[CurvePiece::resolution] {};

void SimpleCurveSampler::ensureTransferTable() {
    static bool computed = false;
    if (computed) { return; }

    const float pi     = MathConstants<float>::pi;
    const double isize = 1.0 / double(CurvePiece::resolution);

    for (int i = 0; i < CurvePiece::resolution; ++i) {
        double x         = i * isize;
        transferTable[i] = float(
            x
            - 0.2180285 * std::sin(2.0 * pi * x)
            + 0.0322599 * std::sin(4.0 * pi * x)
            - 0.0018794 * std::sin(6.0 * pi * x)
        );
    }

    computed = true;
}

void SimpleCurveSampler::placeWaveBuffers(int size) {
    constexpr int numBuffers = 5;
    memoryBuffer.ensureSize(size * numBuffers);

    waveX = memoryBuffer.place(size);
    waveY = memoryBuffer.place(size);
    diffX = memoryBuffer.place(size);
    slope = memoryBuffer.place(size);
    area  = memoryBuffer.place(size);
}

void SimpleCurveSampler::buildFromCurves(const std::vector<CurvePiece>& curves) {
    if (curves.size() < 2) {
        cleanUp();
        return;
    }

    ensureTransferTable();

    const int res = CurvePiece::resolution / 2;
    int totalRes  = 0;

    // Determine per-piece resolution and total size
    for (size_t i = 0; i < curves.size() - 1; ++i) {
        const CurvePiece& a = curves[i];
        const CurvePiece& b = curves[i + 1];
        const int thisRes   = res >> a.resIndex;
        const int nextRes   = res >> b.resIndex;
        const int curveRes  = jmin(thisRes, nextRes);
        totalRes += curveRes;
    }

    placeWaveBuffers(totalRes);

    zeroIndex   = 0;
    oneIndex    = INT_MAX / 2;
    int waveIdx = 0;
    int cumeRes = 0;

    for (size_t c = 0; c < curves.size() - 1; ++c) {
        const CurvePiece& thisCurve = curves[c];
        const CurvePiece& nextCurve = curves[c + 1];

        int indexA          = 0, indexB = 0;
        const int curveRes  = jmin(res >> thisCurve.resIndex, res >> nextCurve.resIndex);
        const int offset    = res >> thisCurve.resIndex;
        const int xferInc   = CurvePiece::resolution / curveRes;
        const int thisShift = jmax(0, (nextCurve.resIndex - thisCurve.resIndex));
        const int nextShift = jmax(0, (thisCurve.resIndex - nextCurve.resIndex));

        for (int i = 0; i < curveRes; ++i) {
            const float xferValue = transferTable[i * xferInc];
            indexA                = (i << thisShift) + offset;
            indexB                = (i << nextShift);

            const float t1x = thisCurve.transformX[indexA] * (1 - xferValue);
            const float t1y = thisCurve.transformY[indexA] * (1 - xferValue);
            const float t2x = nextCurve.transformX[indexB] * xferValue;
            const float t2y = nextCurve.transformY[indexB] * xferValue;

            waveX[waveIdx] = t1x + t2x;
            waveY[waveIdx] = t1y + t2y;
            ++waveIdx;
        }

        if (cumeRes > 0 && waveX[cumeRes - 1] <= 0 && waveX[cumeRes] > 0) {
            zeroIndex = cumeRes - 1;
        } else if (waveX[cumeRes] <= 0 && waveX[cumeRes + curveRes - 1] > 0) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] <= 0 && waveX[i + 1] > 0) {
                    zeroIndex = i;
                    break;
                }
            }
        }

        if (cumeRes > 0 && waveX[cumeRes - 1] < 1 && waveX[cumeRes] >= 1) {
            oneIndex = cumeRes - 1;
        } else if (waveX[cumeRes] < 1 && waveX[cumeRes + curveRes - 1] >= 1) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] < 1 && waveX[i + 1] >= 1) {
                    oneIndex = i;
                    break;
                }
            }
        }

        cumeRes += curveRes;
    }

    if (cumeRes == 0) {
        cleanUp();
        return;
    }

    const int resSubOne = cumeRes - 1;
    Buffer<float> slp   = slope.withSize(resSubOne);
    Buffer<float> dif   = diffX.withSize(resSubOne);
    Buffer<float> are   = area.withSize(resSubOne);

    VecOps::sub(waveX, waveX + 1, dif);
    VecOps::sub(waveY, waveY + 1, slp);
    VecOps::sub(waveY, waveY + 1, are);
    dif.threshLT(1e-6f);
    slp.div(dif);
    are.mul(dif).mul(0.5f);
}

float SimpleCurveSampler::sampleAt(double position) {
    if (!isSampleable()) { return 0.f; }

    int idx = Arithmetic::binarySearch((float) position, waveX);
    if (idx <= 0) { return waveY.front(); }
    if (idx >= waveX.size()) { return waveY.back(); }

    return float((position - waveX[idx - 1]) * slope[idx - 1] + waveY[idx - 1]);
}

void SimpleCurveSampler::sampleToBuffer(Buffer<float>& buffer, double delta) {
    if (buffer.size() <= 0) { return; }

    if (!isSampleable()) {
        buffer.set(0.5f);
        return;
    }

    float* out       = buffer.get();
    double phase     = 0.0;
    int currentIndex = jmax(0, zeroIndex - 1);

    while (phase < waveX[currentIndex] && currentIndex > 0) {
        --currentIndex;
    }

    for (int i = 0; i < buffer.size(); ++i) {
        while (phase >= waveX[currentIndex + 1]) {
            ++currentIndex;
        }

        out[i] = static_cast<float>((phase - waveX[currentIndex]) * slope[currentIndex] + waveY[currentIndex]);
        phase += delta;
    }
}

void SimpleCurveSampler::cleanUp() {
    waveX.nullify();
    waveY.nullify();
    diffX.nullify();
    slope.nullify();
    area.nullify();
}
