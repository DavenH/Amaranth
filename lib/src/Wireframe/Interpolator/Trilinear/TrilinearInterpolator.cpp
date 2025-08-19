#include "TrilinearInterpolator.h"

#include <Wireframe/Positioner/PathDeformingPositioner.h>

#include "Util/NumberUtils.h"
#include "Wireframe/Env/EnvelopeMesh.h"
#include "Wireframe/Positioner/CyclicPointPositioner.h"
#include "Wireframe/Rasterizer/RasterizerParams.h"
#include "Wireframe/Vertex/Vertex2.h"

TrilinearInterpolator::TrilinearInterpolator(PathDeformingPositioner* pathDeformer)
    : pathDeformer(pathDeformer) {
}

/**
 * In mid-refactor state. Bunch of properties and symbol references not yet carried over from OldMeshRasterizer.
 *
 *
 * @param mesh
 * @return
 */
vector<Intercept> TrilinearInterpolator::interpolate(Mesh* mesh, const RasterizerParams& params) override {
    if (!mesh || mesh->getNumCubes() == 0) {
        return {};
    }

    int zDim = Vertex::Time;
    float zDimMorphPos = morph[zDim];

    Vertex middle;
    vector<Intercept> controlPoints;

    for (auto cube : mesh->getCubes()) {
        cube->getInterceptsFast(zDim, reduct, morph);

        // "point overlaps" means morph position's lies somewhere inside the cube's RYB box
        if (!reduct.pointOverlaps) {
            continue;
        }

        Vertex* a      = &reduct.v0;
        Vertex* b      = &reduct.v1;
        Vertex* vertex = &reduct.v;

        if(params.positionerParams.cyclic) {
            CyclicPointPositioner::wrapTrilinearEdge(
                a->values[zDim], a->values[Vertex::Phase],
                b->values[zDim], b->values[Vertex::Phase],
                zDimMorphPos
            );
        }

        TrilinearCube::vertexAt(zDimMorphPos, zDim, a, b, vertex);

        float x = vertex->values[Vertex::Phase] + params.interpolation.phaseOffset;

        // I want this factored somewhere else - this wrapping policy stuff is for cyclic only.
        // is it possible to actually delay this to the next pipeline stage?
        if (params.positionerParams.cyclic) {
            CyclicPointPositioner::wrapAndDidAnything(x);
        } else {
            NumberUtils::constrain(x, params.positionerParams.minX, params.positionerParams.maxX);
        }

        Intercept intercept(x, vertex->values[Vertex::Amp], cube);
        intercept.shp       = vertex->values[Vertex::Curve];
        intercept.adjustedX = intercept.x;

        // applies position adjustments due to Curve Paths for each dimension, can this be separated into a PointPositioner?
        pathDeformer->applyPathDeformToIntercept(intercept, morph, params.positionerParams);
        controlPoints.emplace_back(intercept);
    }

    return controlPoints;
}
