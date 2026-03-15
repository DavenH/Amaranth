#include "V2WaveBuilderStages.h"

#include <climits>
#include <cmath>

#include <Array/ScopedAlloc.h>
#include <Array/VecOps.h>
#include <App/AppConstants.h>
#include <Curve/GuideCurveProvider.h>

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

int computeCurveResolution(
    const std::vector<Curve>& curves,
    int curveIndex,
    int halfRes,
    const V2WaveBuilderContext& context) {
    const Curve& thisCurve = curves[curveIndex];
    const Curve& nextCurve = curves[curveIndex + 1];

    int thisRes = halfRes >> thisCurve.resIndex;
    int nextRes = halfRes >> nextCurve.resIndex;

    VertCube* cube = thisCurve.b.cube;
    if (! context.componentPath.enabled
            || context.componentPath.path == nullptr
            || cube == nullptr
            || cube->getCompGuideCurve() < 0) {
        return jmin(thisRes, nextRes);
    }

    int compDfrm = cube->getCompGuideCurve();
    int numVerts = context.componentPath.path->getTableDensity(compDfrm);
    int desiredRes = thisRes * static_cast<int>(((context.componentPath.lowResolution ? 2.0f : 8.0f)
        * std::sqrt(static_cast<float>(numVerts))) + 0.49f);

    const int tableSize = GuideCurveProvider::tableSize;
    if (desiredRes <= 0) {
        return jmin(thisRes, nextRes);
    }

    float scaleRatio = tableSize / static_cast<float>(desiredRes);
    if (curves.size() == 6u) {
        scaleRatio /= 2.0f;
    }

    int truncRatio = jlimit(1, 256, static_cast<int>(scaleRatio + 0.5f));
    return tableSize / truncRatio;
}

void appendDecoupledRegion(
    const Curve& thisCurve,
    const Curve& nextCurve,
    float amplitude,
    const V2WaveBuilderContext& context) {
    if (context.componentPath.deformRegions == nullptr) {
        return;
    }

    V2DeformRegion region;
    region.amplitude = amplitude;
    region.deformChan = thisCurve.b.cube->getCompGuideCurve();
    region.start = thisCurve.b;
    region.end = nextCurve.b;
    context.componentPath.deformRegions->emplace_back(region);
}

void renderCurveWithComponentDeformer(
    const Curve& thisCurve,
    const Curve& nextCurve,
    int curveRes,
    int waveIdx,
    Buffer<float> waveX,
    Buffer<float> waveY,
    const V2WaveBuilderContext& context) {
    GuideCurveProvider::NoiseContext noise;
    noise.noiseSeed = context.componentPath.noiseSeed < 0
        ? static_cast<int>(context.componentPath.morphTime * INT_MAX)
        : context.componentPath.noiseSeed;

    int compDfrm = thisCurve.b.cube->getCompGuideCurve();
    noise.phaseOffset = context.componentPath.phaseOffsetSeeds[compDfrm];
    noise.vertOffset = context.componentPath.vertOffsetSeeds[compDfrm];

    Buffer<float> yPortion(waveY + waveIdx, curveRes);
    Buffer<float> xPortion(waveX + waveIdx, curveRes);

    float multiplier = thisCurve.b.shp * thisCurve.b.cube->guideCurveAbsGain(Vertex::Time);
    if (context.componentPath.decoupled) {
        yPortion.zero();
        appendDecoupledRegion(thisCurve, nextCurve, multiplier, context);
    } else {
        context.componentPath.path->sampleDownAddNoise(compDfrm, yPortion, noise);
        yPortion.mul(multiplier);
    }

    float invSize = 1.0f / static_cast<float>(curveRes);

    float ySlope = invSize * (nextCurve.b.y - thisCurve.b.y);
    Buffer<float> ramp = xPortion;
    ramp.ramp(thisCurve.b.y, ySlope);
    yPortion.add(ramp);

    float minX = jmin(thisCurve.b.x, nextCurve.b.x);
    float maxX = jmax(thisCurve.b.x, nextCurve.b.x);
    float xSlope = (maxX - minX) * invSize;
    xPortion.ramp(minX, xSlope);
}

void renderCurveByBlend(
    const Curve& thisCurve,
    const Curve& nextCurve,
    int curveRes,
    int halfRes,
    int waveIdx,
    const Buffer<float>& transferTable,
    Buffer<float> waveX,
    Buffer<float> waveY) {
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

        waveX[waveIdx + i] = t1x + t2x;
        waveY[waveIdx + i] = t1y + t2y;
    }
}

}

bool V2DefaultWaveBuilderStage::run(
        const std::vector<Curve>& curves,
        int curveCount,
        V2WaveBuffers waveBuffers,
        int& outWavePointCount,
        int& zeroIndex,
        int& oneIndex,
        const V2WaveBuilderContext& context) noexcept {
    Buffer<float> waveX = waveBuffers.waveX;
    Buffer<float> waveY = waveBuffers.waveY;
    Buffer<float> diffX = waveBuffers.diffX;
    Buffer<float> slope = waveBuffers.slope;

    outWavePointCount = 0;
    zeroIndex = 0;
    oneIndex = INT_MAX / 2;

    if (curveCount < 2 || curveCount > static_cast<int>(curves.size())) {
        return false;
    }

    if (context.componentPath.enabled
            && (context.componentPath.path == nullptr
                || context.componentPath.vertOffsetSeeds == nullptr
                || context.componentPath.phaseOffsetSeeds == nullptr)) {
        return false;
    }

    if (context.componentPath.deformRegions != nullptr) {
        context.componentPath.deformRegions->clear();
    }

    const Buffer<float> transferTable = getTransferTable();

    const int halfRes = Curve::resolution / 2;
    std::vector<int> curveResolution;
    curveResolution.reserve(static_cast<size_t>(curveCount - 1));

    int totalRes = 0;
    for (int i = 0; i < curveCount - 1; ++i) {
        int res = computeCurveResolution(curves, i, halfRes, context);
        curveResolution.emplace_back(res);
        totalRes += res;
    }

    if (totalRes <= 1 || totalRes > waveX.size() || totalRes > waveY.size()) {
        return false;
    }

    waveX.withSize(totalRes).zero();
    waveY.withSize(totalRes).zero();

    int waveIdx = 0;
    int cumeRes = 0;
    for (int c = 0; c < curveCount - 1; ++c) {
        const Curve& thisCurve = curves[c];
        const Curve& nextCurve = curves[c + 1];
        int curveRes = curveResolution[c];

        bool hasComponentDeformer = context.componentPath.enabled
            && context.componentPath.path != nullptr
            && thisCurve.b.cube != nullptr
            && thisCurve.b.cube->getCompGuideCurve() >= 0;

        if (hasComponentDeformer) {
            renderCurveWithComponentDeformer(thisCurve, nextCurve, curveRes, waveIdx, waveX, waveY, context);
            waveIdx += curveRes;
        } else {
            renderCurveByBlend(thisCurve, nextCurve, curveRes, halfRes, waveIdx, transferTable, waveX, waveY);
            waveIdx += curveRes;
        }

        if (cumeRes > 0 && waveX[cumeRes - 1] <= 0.0f && waveX[cumeRes] > 0.0f) {
            zeroIndex = cumeRes - 1;
        } else if (waveX[cumeRes] <= 0.0f && waveX[cumeRes + curveRes - 1] > 0.0f) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] <= 0.0f && waveX[i + 1] > 0.0f) {
                    zeroIndex = i;
                    break;
                }
            }
        }

        if (cumeRes > 0 && waveX[cumeRes - 1] < 1.0f && waveX[cumeRes] >= 1.0f) {
            oneIndex = cumeRes - 1;
        } else if (waveX[cumeRes] < 1.0f && waveX[cumeRes + curveRes - 1] >= 1.0f) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] < 1.0f && waveX[i + 1] >= 1.0f) {
                    oneIndex = i;
                    break;
                }
            }
        }

        cumeRes += curveRes;
    }

    outWavePointCount = waveIdx;
    if (outWavePointCount <= 1) {
        return false;
    }

    if (zeroIndex == 0 && waveX.front() < 0.0f && waveX[outWavePointCount - 1] > 0.0f) {
        for (int i = 0; i < outWavePointCount - 1; ++i) {
            if (waveX[i] <= 0.0f && waveX[i + 1] > 0.0f) {
                zeroIndex = i;
                break;
            }
        }
    }

    if (oneIndex >= outWavePointCount - 1) {
        oneIndex = outWavePointCount - 2;
        for (int i = 0; i < outWavePointCount - 1; ++i) {
            if (waveX[i] < 1.0f && waveX[i + 1] >= 1.0f) {
                oneIndex = i;
                break;
            }
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
