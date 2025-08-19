#include "VisualTrilinearInterpolator.h"

#include <Wireframe/Mesh.h>
#include <Wireframe/Positioner/CyclicPointPositioner.h>
#include <Wireframe/Positioner/PathDeformingPositioner.h>
#include <Wireframe/Rasterizer/RasterizerParams.h>
#include <Wireframe/Vertex/Vertex2.h>

VisualTrilinearInterpolator::VisualTrilinearInterpolator(PathDeformingPositioner* pathDeformer)
    : pathDeformer(pathDeformer) {
}

vector<TrilinearEdge> VisualTrilinearInterpolator::interpolate(Mesh* mesh, const RasterizerParams& params) override {
    float poleA[3];
    float poleB[3];
    int zDim = Vertex::Time;
    float zDimMorphPos = morph[zDim];
    constexpr int independentDims[3] = { Vertex::Time, Vertex::Red, Vertex::Blue };

    Vertex middle;
    Intercept midIcpt;
    vector<TrilinearEdge> edges;

    pathDeformer->setMorph(morph);

    for (auto cube : mesh->getCubes()) {
        cube->getInterceptsFast(zDim, reduct, morph);

        // "point overlaps" means morph position's lies somewhere inside the cube's RYB box
        if (! reduct.pointOverlaps) {
            continue;
        }

        Vertex* a      = &reduct.v0;
        Vertex* b      = &reduct.v1;
        Vertex* vertex = &reduct.v;

        const Dimensions& dims = params.interpolation.dims;

        if(params.positionerParams.cyclic) {
            CyclicPointPositioner::wrapTrilinearEdge(
                a->values[zDim], a->values[Vertex::Phase],
                b->values[zDim], b->values[Vertex::Phase],
                zDimMorphPos
            );
        }

        TrilinearCube::vertexAt(zDimMorphPos, zDim, a, b, vertex);

        midIcpt = Intercept(
            vertex->values[Vertex::Phase] + params.interpolation.phaseOffset,
            vertex->values[Vertex::Amp],
            cube
        );

        // i.e. which one is the x-axis of our "3D" view panels
        // why does this need to be a method as opposed to property? It's overridden somewhere? GraphicXyzRasterizers?
        int currentlyVisibleRYBDim = getPrimaryViewDimension();

        // sets adjustedX
        pathDeformer->applyPathDeformToIntercept(midIcpt, morph, params.positionerParams);

        for (int i = 0; i < dims.numHidden(); ++i) {
            Vertex2 midCopy(midIcpt.adjustedX, midIcpt.y);

            // hidden dims are those interpolation dims (red/yellow/blue) which are NOT the primary view dimension,
            // e.g. if yellow is the 3D panels' x-axis, red/blue are the hidden dims
            int hiddenDim = dims.hidden[i];

            // feels like a helper method in MorphPosition
            for (int j = 0; j < numElementsInArray(independentDims); ++j) {
                poleA[j] = hiddenDim == independentDims[j] ? 0.f : morph[independentDims[j]];
                poleB[j] = hiddenDim == independentDims[j] ? 1.f : morph[independentDims[j]];
            }

            // put the morph cursor at the corners of the cube because in this operation we want to visualize
            // the edges of the Trilinear Cube -- depth verts are either ends of these edges
            MorphPosition posA(poleA[0], poleA[1], poleA[2]);
            cube->getFinalIntercept(reduct, posA);

            Intercept beforeIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);

            pathDeformer->applyPathDeformToIntercept(beforeIcpt, posA, params.positionerParams
                                                   , hiddenDim == currentlyVisibleRYBDim);

            MorphPosition posB(poleB[0], poleB[1], poleB[2]);
            cube->getFinalIntercept(reduct, posB);

            Intercept afterIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);

            pathDeformer->applyPathDeformToIntercept(afterIcpt, posB, params.positionerParams
                                                   , hiddenDim == currentlyVisibleRYBDim);

            Vertex2 before(beforeIcpt.adjustedX, beforeIcpt.y);
            Vertex2 after(afterIcpt.adjustedX, afterIcpt.y);

            // the problem here is that this is also leaking positioner behavior, but also mixing it with interpolator
            // behavior -- we create new points IFF we are cyclic and the points transit an edge boundary

            // maybe we CAN wait until the next pipeline stage, are we actually losing any information?

            // Yeah we have to insert a virtual point, buuut maybe making the output point list mutable is a sacrifice
            // we have to make. But mutable generic types... nah
            if (params.positionerParams.cyclic) {
                // if this pair transits the x=1 boundary
                if ((before.x > 1) != (after.x > 1)) {
                    Vertex2 before2 = before;
                    Vertex2 after2  = after;
                    Vertex2 mid2    = midCopy;

                    after2.x  -= 1.f;
                    before2.x -= 1.f;
                    mid2.x    -= 1.f;

                    edges.emplace_back(cube, before2, mid2, after2, hiddenDim);
                }

                else if (before.x > 1 && after.x > 1) {
                    after.x   -= 1.f;
                    before.x  -= 1.f;
                    midCopy.x -= 1.f;
                }
            }


            edges.emplace_back(cube, before, midCopy, after, hiddenDim);
        }
    }

    return edges;
}
