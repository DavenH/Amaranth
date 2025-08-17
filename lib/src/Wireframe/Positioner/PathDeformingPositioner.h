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
            return;
        }

        for (Intercept& icpt : controlPoints) {
            if (icpt.cube == nullptr) {
                continue;
            }
            TrilinearCube* cube = icpt.cube;

            IPathSampler::NoiseContext noise;
            noise.noiseSeed = noiseSeed;

            if (int chan = cube->pathAt(Vertex::Red) >= 0) {
                float progress    = cube->getPortionAlong(Vertex::Red, morph);
                noise.vertOffset  = vertOffsetSeeds[chan];
                noise.phaseOffset = phaseOffsetSeeds[chan];
                icpt.adjustedX += cube->pathAbsGain(Vertex::Red) * pathImpl->getTableValue(chan, progress, noise);
            }

            if (int chan = cube->pathAt(Vertex::Blue) >= 0) {
                float progress    = cube->getPortionAlong(Vertex::Blue, morph);
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
                continue;
            }

            if (int chan = cube->pathAt(Vertex::Amp) >= 0) {
                noise.vertOffset  = vertOffsetSeeds[chan];
                noise.phaseOffset = phaseOffsetSeeds[chan];
                icpt.y += cube->pathAbsGain(Vertex::Amp) * pathImpl->getTableValue(chan, progress, noise);

                // TODO: we don't want to do this here; but verify that it's applied in the curve generator before deleting
                // NumberUtils::constrain(icpt.y, (scaling != Unipolar ? -1.f : 0.f), 1.f);
            }

            if (int chan = cube->pathAt(Vertex::Phase) >= 0) {
                noise.vertOffset  = vertOffsetSeeds[chan];
                noise.phaseOffset = phaseOffsetSeeds[chan];

                icpt.adjustedX += cube->pathAbsGain(Vertex::Phase) * pathImpl->getTableValue(chan, progress, noise);
                // these cyclic adjustments need concentrating into a single policy
                if (config.cyclic) {
                    float lastAdjX = icpt.adjustedX;

                    // TODO: simple wrapping policy
                    while (icpt.adjustedX >= 1.f) icpt.adjustedX -= 1.f;
                    while (icpt.adjustedX < 0.f) icpt.adjustedX += 1.f;

                    if (lastAdjX != icpt.adjustedX) {
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
    }

private:
    IPathSampler* pathImpl {};
    bool decoupleComponentPaths {};
    bool needsResorting {};
    int noiseSeed {};
    short phaseOffsetSeeds[128] {};
    short vertOffsetSeeds[128] {};
    MorphPosition morph {};
    TrilinearCube::ReductionData reduct {};
};
