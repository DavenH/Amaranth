#pragma once
#include "Util/NumberUtils.h"

#include "PointPositioner.h"
#include "../Path/ICurvePath.h"

/**
 * Applies path path adjustments upon control points
 */
class DeformingPositioner : public PointPositioner {
public:
    void adjustControlPoints(vector<Intercept>& controlPoints, CurveParameters config) override {
        for(Intercept& icpt : controlPoints) {
            if(icpt.cube == nullptr || path == nullptr) {
            return;
        }

        TrilinearCube* cube = icpt.cube;

        ICurvePath::NoiseContext noise;
        noise.noiseSeed = noiseSeed;

        if (cube->pathAt(Vertex::Red) >= 0) {
            int path = cube->pathAt(Vertex::Red);
            float progress = cube->getPortionAlong(Vertex::Red, morph);

            noise.vertOffset  = vertOffsetSeeds [path];
            noise.phaseOffset = phaseOffsetSeeds[path];

            icpt.adjustedX += cube->pathAbsGain(Vertex::Red) * path->getTableValue(path, progress, noise);
        }

        if (cube->pathAt(Vertex::Blue) >= 0) {
            int path = cube->pathAt(Vertex::Blue);
            float progress = cube->getPortionAlong(Vertex::Blue, morph);

            noise.vertOffset  = vertOffsetSeeds [path];
            noise.phaseOffset = phaseOffsetSeeds[path];

            icpt.adjustedX += cube->pathAbsGain(Vertex::Blue) * path->getTableValue(path, progress, noise);
        }

        float timeMin = reduct.v0.values[Vertex::Time];
        float timeMax = reduct.v1.values[Vertex::Time];

        if(timeMin > timeMax) {
            std::swap(timeMin, timeMax);
        }

        float diffTime = timeMax - timeMin;
        float progress = diffTime == 0.f ? 0.f : fabsf(reduct.v.values[Vertex::Time] - timeMin) / diffTime;

        bool ignore = (noOffsetAtEnds && (progress == 0 || progress == 1.f));

        if (cube->pathAt(Vertex::Amp) >= 0) {
            int pathChan = cube->pathAt(Vertex::Amp);
            noise.vertOffset  = vertOffsetSeeds [pathChan];
            noise.phaseOffset = phaseOffsetSeeds[pathChan];

            if (!ignore) {
                icpt.y += cube->pathAbsGain(Vertex::Amp) * path->getTableValue(pathChan, progress, noise);
                NumberUtils::constrain(icpt.y, (config.scalingType != Unipolar ? -1.f : 0.f), 1.f);
            }
        }

        if (cube->pathAt(Vertex::Phase) >= 0) {
            int pathChan = cube->pathAt(Vertex::Phase);
            noise.vertOffset  = vertOffsetSeeds [pathChan];
            noise.phaseOffset = phaseOffsetSeeds[pathChan];

            if (!ignore) {
                icpt.adjustedX += cube->pathAbsGain(Vertex::Phase) * path->getTableValue(pathChan, progress, noise);

                if (config.cyclic) {
                    float lastAdjX = icpt.adjustedX;

                    while(icpt.adjustedX >= 1.f)     { icpt.adjustedX -= 1.f; }
                    while(icpt.adjustedX < 0.f)     { icpt.adjustedX += 1.f; }

                    if (lastAdjX != icpt.adjustedX) {
                        icpt.isWrapped = true;
                        needsResorting = true;
                    }
                }
            }
        }

        if (cube->pathAt(Vertex::Curve) >= 0) {
            int path = cube->pathAt(Vertex::Curve);
            noise.vertOffset  = vertOffsetSeeds[path];
            noise.phaseOffset = phaseOffsetSeeds[path];

            icpt.shp += 2 * cube->pathAbsGain(Vertex::Curve) * path->getTableValue(path, progress, noise);

            NumberUtils::constrain(icpt.shp, 0.f, 1.f);
        }
        }
    }

private:
    ICurvePath* path;
    bool decoupleComponentPaths;

    int noiseSeed;
    short phaseOffsetSeeds[128];
    short vertOffsetSeeds[128];
};