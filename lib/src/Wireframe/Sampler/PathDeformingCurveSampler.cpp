#include "PathDeformingCurveSampler.h"
#include "../../App/AppConstants.h"
#include "../../Util/NumberUtils.h"
#include "JuceHeader.h"
#include "Wireframe/Path/IPathSampler.h"
#include "Wireframe/Rasterizer/RasterizerParams.h"
#include <climits>

PathDeformingCurveSampler::PathDeformingCurveSampler(IPathSampler* path)
    : path(path) {
    jassert(path != nullptr);
}

void PathDeformingCurveSampler::adjustDeformingSharpness(std::vector<CurvePiece>& curves) {
    for (int i = 0; i < (int) curves.size(); ++i) {
        CurvePiece& curve = curves[i];

        if (i < (int) curves.size() - 1 && curve.b.cube != nullptr) {
            if (curve.b.cube->getCompPath() >= 0) {
                CurvePiece& next = curves[i + 1];

                if (next.b.cube == nullptr || next.b.cube->getCompPath() < 0) {
                    curve.c.shp = 1;
                    next.b.shp  = 1;
                    next.updateCurrentIndex();

                    if (i < (int) curves.size() - 2) {
                        curves[i + 2].a.shp = 1;
                    }
                }
            }
        }
    }
}

SamplerOutput PathDeformingCurveSampler::buildFromCurves(
    std::vector<CurvePiece>& curves,
    const SamplingParameters& params) override {
    if (curves.size() < 2) {
        cleanUp();
        return {};
    }

    adjustDeformingSharpness(curves);

    if (decoupleComponentPaths) {
        deformRegions.clear();
    }

    return SimpleCurveSampler::buildFromCurves(curves, params);
}

int PathDeformingCurveSampler::getCurveResolution(const CurvePiece& thisCurve, const CurvePiece& nextCurve) const {
    TrilinearCube* cube = thisCurve.b.cube;

    if (cube != nullptr && path != nullptr && cube->getCompPath() >= 0) {
        const int res    = CurvePiece::resolution / 2;
        int thisRes      = res >> thisCurve.resIndex;
        int tableSize    = Constants::DeformTableSize;
        int numVerts     = path->getTableDensity(cube->getCompPath());
        int desiredRes   = thisRes * (int) (8 * sqrtf(numVerts) + 0.49f);
        float scaleRatio = tableSize / float(desiredRes);
        int truncRatio   = jlimit(1, 256, roundToInt(scaleRatio));
        return tableSize / truncRatio;
    }

    return SimpleCurveSampler::getCurveResolution(thisCurve, nextCurve);
}

void PathDeformingCurveSampler::generateWaveformForCurve(int& waveIdx, const CurvePiece& thisCurve
                                                       , const CurvePiece& nextCurve, int curveRes) {
    TrilinearCube* cube = thisCurve.b.cube;
    if (cube != nullptr && path != nullptr && cube->getCompPath() >= 0) {
        int compPath = cube->getCompPath();

        const Intercept& thisCentre = thisCurve.b;
        const Intercept& nextCentre = nextCurve.b;

        IPathSampler::NoiseContext noise;
        noise.noiseSeed   = noiseSeed < 0 ? 0 : noiseSeed;
        noise.phaseOffset = phaseOffsetSeeds[compPath];
        noise.vertOffset  = vertOffsetSeeds[compPath];

        Buffer<Float32> yPortion(waveY.get() + waveIdx, curveRes);
        Buffer<Float32> xPortion(waveX.get() + waveIdx, curveRes);

        float multiplier = thisCentre.shp * cube->pathAbsGain(Vertex::Time);

        if (decoupleComponentPaths) {
            yPortion.zero();

            DeformRegion region;
            region.amplitude  = multiplier;
            region.deformChan = cube->getCompPath();
            region.start      = thisCentre;
            region.end        = nextCentre;

            deformRegions.emplace_back(region);
        } else {
            path->sampleDownAddNoise(cube->getCompPath(), yPortion, noise);
            yPortion.mul(multiplier);
        }

        float invSize    = 1.0f / float(curveRes);
        float curveSlope = invSize * (nextCentre.y - thisCentre.y);

        Buffer<float> temp = xPortion;
        temp.ramp(thisCentre.y, curveSlope);
        yPortion.add(temp);

        float minX = jmin(thisCentre.x, nextCentre.x);
        float maxX = jmax(thisCentre.x, nextCentre.x);

        curveSlope = (maxX - minX) * invSize;
        xPortion.ramp(minX, curveSlope);

        waveIdx += curveRes;
    } else {
        SimpleCurveSampler::generateWaveformForCurve(waveIdx, thisCurve, nextCurve, curveRes);
    }
}

float PathDeformingCurveSampler::sampleAtDecoupled(double angle, DeformContext& context) {
    float val = sampleAt(angle);

    for (auto& region: deformRegions) {
        if (NumberUtils::within<float>(angle, region.start.x, region.end.x)) {
            float diff = region.end.x - region.start.x;

            if (diff > 0) {
                IPathSampler::NoiseContext noise;
                noise.noiseSeed   = noiseSeed;
                noise.phaseOffset = context.phaseOffsetSeed;
                noise.vertOffset  = context.vertOffsetSeed;

                float progress = (angle - region.start.x) / diff;

                return val + region.amplitude * path->getTableValue(region.deformChan, progress, noise);
            }
        }
    }

    return val;
}
