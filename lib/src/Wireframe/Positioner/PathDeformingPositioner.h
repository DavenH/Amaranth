#pragma once

#include "Util/NumberUtils.h"
#include "PointPositioner.h"

#include "../Interpolator/Trilinear/MorphPosition.h"
#include "../Interpolator/Trilinear/TrilinearCube.h"
#include "../Path/IPathSampler.h"

// Applies path-driven deformations (amp/phase/curve/time/red/blue) to control points
class PathDeformingPositioner : public PointPositioner {
public:
    void setPath(IPathSampler* p) { pathImpl = p; }

    // The noise seed needs updating every cycle, when it comes to VoiceMeshRasterizer.
    // Graphic Rasterizers need to update it when the playback cursor moves
    void setNoiseSeed(int seed) { noiseSeed = seed; }
    void setDecoupleComponentPaths(bool d) { decoupleComponentPaths = d; }
    void setMorph(const MorphPosition& m) { morph = m; }

    void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config) override {
        if (!pathImpl) {
            throw std::runtime_error("PathDeformingPositioner::adjustControlPoints: pathImpl is null");
        }

        for (Intercept& icpt : controlPoints) {
            applyPathDeformToIntercept(icpt, this->morph, config);
        }
    }

    void applyPathDeformToIntercept(
        Intercept& icpt,
        const MorphPosition& morphPos,
        PositionerParameters params,
        bool noOffsetAtEnds = false

    ) {
        if (!pathImpl || icpt.cube == nullptr) {
            return;
        }
        TrilinearCube* cube = icpt.cube;

        IPathSampler::NoiseContext noise;
        noise.noiseSeed = noiseSeed;

        if (int chan = cube->pathAt(Vertex::Red) >= 0) {
            float progress    = cube->getPortionAlong(Vertex::Red, morphPos);
            noise.vertOffset  = vertOffsetSeeds[chan];
            noise.phaseOffset = phaseOffsetSeeds[chan];
            icpt.adjustedX += cube->pathAbsGain(Vertex::Red) * pathImpl->getTableValue(chan, progress, noise);
        }

        if (int chan = cube->pathAt(Vertex::Blue) >= 0) {
            float progress    = cube->getPortionAlong(Vertex::Blue, morphPos);
            noise.vertOffset  = vertOffsetSeeds[chan];
            noise.phaseOffset = phaseOffsetSeeds[chan];
            icpt.adjustedX += cube->pathAbsGain(Vertex::Blue) * pathImpl->getTableValue(chan, progress, noise);
        }

        float timeMin = reduct.v0.values[Vertex::Time];
        float timeMax = reduct.v1.values[Vertex::Time];

        if (timeMin > timeMax) {
            std::swap(timeMin, timeMax);
        }

        float diffTime = timeMax - timeMin;
        float progress = diffTime == 0.f ? 0.f : fabsf(reduct.v.values[Vertex::Time] - timeMin) / diffTime;
        bool ignore    = (noOffsetAtEnds && (progress == 0 || progress == 1.f));

        if (ignore) {
            return;
        }

        if (int chan = cube->pathAt(Vertex::Amp) >= 0) {
            noise.vertOffset  = vertOffsetSeeds[chan];
            noise.phaseOffset = phaseOffsetSeeds[chan];
            icpt.y += cube->pathAbsGain(Vertex::Amp) * pathImpl->getTableValue(chan, progress, noise);
        }

        if (int chan = cube->pathAt(Vertex::Phase) >= 0) {
            noise.vertOffset  = vertOffsetSeeds[chan];
            noise.phaseOffset = phaseOffsetSeeds[chan];

            icpt.adjustedX += cube->pathAbsGain(Vertex::Phase) * pathImpl->getTableValue(chan, progress, noise);
            // these cyclic adjustments need concentrating into a single policy
            if (params.cyclic) {
                if (CyclicPointPositioner::wrapAndDidAnything(icpt.adjustedX)) {
                    icpt.isWrapped = true;
                    needsResorting = true;
                }
            }
        }

        if (int chan = cube->pathAt(Vertex::Curve) >= 0) {
            noise.vertOffset  = vertOffsetSeeds[chan];
            noise.phaseOffset = phaseOffsetSeeds[chan];

            icpt.shp += 2 * cube->pathAbsGain(Vertex::Curve) * pathImpl->getTableValue(chan, progress, noise);
            NumberUtils::constrain(icpt.shp, 0.f, 1.f);
        }
    }

private:
    IPathSampler* pathImpl {};
    // we need to rename this. I have no idea what Components are here. Probably a search/replace error.
    bool decoupleComponentPaths {};
    bool needsResorting {};
    int noiseSeed {};
    short phaseOffsetSeeds[128] {};
    short vertOffsetSeeds[128] {};
    MorphPosition morph {};
    TrilinearCube::ReductionData reduct {};
};
