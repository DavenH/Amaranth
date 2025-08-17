#include "TrilinearInterpolator.h"

#include <Wireframe/Positioner/PathDeformingPositioner.h>

#include "Util/NumberUtils.h"
#include "Wireframe/Env/EnvelopeMesh.h"
#include "Wireframe/Positioner/CyclicPointPositioner.h"
#include "Wireframe/State/RasterizerParameters.h"
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
vector<Intercept> TrilinearInterpolator::interpolate(Mesh* mesh, const RasterizerParameters& params) override {
    if (!mesh || mesh->getNumCubes() == 0) {
        return {};
    }

    int zDim = Vertex::Time;
    float zDimMorphPos = morph[zDim];

    Vertex middle;

    vector<Intercept> controlPoints;

    for (auto cube : mesh->getCubes()) {
        // this is a weirdly named method. Get Intercepts? It should be more like calcPointOverlap or whatnot.
        // Should we return ReductionData as more pure functional style? I think so.
        // But it does has a lot of vertices, so would be a lot of copying. Performance hit? Probably negligible.
        // Method evaluates: does tricube contain morph position?
        cube->getInterceptsFast(zDim, reduct, morph);

        // "point overlaps" means morph position's lies somewhere inside the cube's RYB box
        if (!reduct.pointOverlaps) {
            continue;
        }

        Vertex* a      = &reduct.v0;
        Vertex* b      = &reduct.v1;
        Vertex* vertex = &reduct.v;

        // legacy point positioning - does it need to be done before trilinear interpolation?
        // I think it's only valid for cyclic point positioning -- should that be run here, only if configured?
        // used to be:
        //      wrapVertices(a->values[zDim], a->values[Vertex::Phase],
        //                   b->values[zDim], b->values[Vertex::Phase], independent);
        if(params.wrapLinesNotTransitingEdges) {
            // I think this should capture a lot more of the semantics of the operation.
            // We want to preserve lines that transit the edge in a 2D waveshape. We don't want one point being wrapped
            // We DO want both points wrapped if neither of them transit the [0, 1] edges, i.e. both are < 0 or > 1
            // now, since we're talking lines, I think we need to have it named wrapLines, or something.
            // Should there be a way to generate Line segments by a->line(zDim, Vertex::phase)?
            // Second thought, this doesn't seem to be a positioner thing necessarily. It's a line splitting policy.
            cyclicPositioner->wrapVertices(
                a->values[zDim], a->values[Vertex::Phase],
                b->values[zDim], b->values[Vertex::Phase],
                zDimMorphPos
            );
        }

        TrilinearCube::vertexAt(zDimMorphPos, zDim, a, b, vertex);

        // osc phase is a leaky, unabstracted property coming from invokers specific to time-domain waveforms.
        // However, defaulting to zero may be acceptable. Maybe just naming it as "phaseOffset"
        // could be abstract enough to allow here.
        float x = vertex->values[Vertex::Phase] + params.interpolation.oscPhase;

        // I want this factored somewhere else - this wrapping policy stuff is for cyclic only.
        if (params.curve.cyclic) {
            while (x >= 1.f) { x -= 1.f; }
            while (x < 0.f) { x += 1.f; }

            jassert(x >= 0.f && x < 1.f);
            jassert(params.curve.maxX == 1.f && params.curve.minX == 0.f);
        } else {
            NumberUtils::constrain(x, params.curve.minX, params.curve.maxX);
        }

        Intercept intercept(x, vertex->values[Vertex::Amp], cube);

        intercept.shp       = vertex->values[Vertex::Curve];
        intercept.adjustedX = intercept.x;

        // applies position adjustments due to Curve Paths for each dimension, can this be separated into a PointPositioner?
        pathDeformer->adjustControlPoints(intercept, params.curve);
        controlPoints.emplace_back(intercept);
    }

    // sorts by x-value, not adjusted x,
    // is it the interpolator's responsibility to sort? I think not. CurveGenerator needs to do it.
    std::sort(controlPoints.begin(), controlPoints.end());

    return controlPoints;
}
