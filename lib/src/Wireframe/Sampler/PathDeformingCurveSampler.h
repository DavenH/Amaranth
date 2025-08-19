#pragma once

#include "SimpleCurveSampler.h"
#include "../Path/IPathSampler.h"

class PathDeformingCurveSampler : public SimpleCurveSampler {
public:
    struct DeformContext {
        int phaseOffsetSeed;
        int vertOffsetSeed;
        int currentIndex;

        DeformContext() : phaseOffsetSeed(0), vertOffsetSeed(0), currentIndex(0) {}
    };

    struct DeformRegion {
        int deformChan;
        float amplitude;
        Intercept start, end;
    };

    explicit PathDeformingCurveSampler(IPathSampler* path);

    SamplerOutput buildFromCurves(std::vector<CurvePiece>& curves, const SamplingParameters& params) override;

    float sampleAtDecoupled(double angle, DeformContext& context);
    void setDecoupleComponentPaths(bool decouple) { decoupleComponentPaths = decouple; }
    void setNoiseSeed(int seed) { noiseSeed = seed; }

protected:
    int getCurveResolution(const CurvePiece& thisCurve, const CurvePiece& nextCurve) const override;
    void generateWaveformForCurve(int& waveIdx, const CurvePiece& thisCurve, const CurvePiece& nextCurve, int curveRes) override;

private:
    void adjustDeformingSharpness(std::vector<CurvePiece>& curves);

    IPathSampler* path;
    std::vector<DeformRegion> deformRegions;

    bool decoupleComponentPaths{false};
    int noiseSeed{-1};
    ScopedAlloc<short> memory;
    short phaseOffsetSeeds[128]{};
    short vertOffsetSeeds[128]{};
};
