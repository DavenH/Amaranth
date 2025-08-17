#include "VisualTrilinearInterpolator.h"
#include <Wireframe/Positioner/PathDeformingPositioner.h>
#include <Wireframe/Positioner/CyclicPointPositioner.h>
#include <Wireframe/State/RasterizerParameters.h>
#include <Wireframe/Vertex/Vertex2.h>
#include <Wireframe/Mesh.h>

VisualTrilinearInterpolator::VisualTrilinearInterpolator(PathDeformingPositioner* pathDeformer)
    : pathDeformer(pathDeformer) {
}

vector<ColorPoint> VisualTrilinearInterpolator::interpolate(Mesh* mesh, const InterpolatorParameters& params) override {
    float poleA[3];
    float poleB[3];
    int independentDims[3] = { Vertex::Time, Vertex::Red, Vertex::Blue };

    Vertex middle;
    Intercept midIcpt;

    vector<ColorPoint> colorPoints;

    pathDeformer->setMorph(morph);

    for (auto cube : mesh->getCubes()) {
        cube->getInterceptsFast(zDim, reduct, morph);
        if (! reduct.pointOverlaps) {
            continue;
        }


        // calc depth dims is for "2D" waveform/spectrum panels that see "into" the zDim.
        // The midIcpt is what is visible in those panels because it's a cross-section along the zDim
        // trilinear interpolator is trying to do a lot of things at once -- serve the "2D", "3D" use cases.
        // perhaps this should be split.
        TrilinearCube::vertexAt(zDimMorphPos, zDim, a, b, vertex);

        midIcpt.x = vertex->values[Vertex::Phase] + oscPhase;
        midIcpt.y = vertex->values[Vertex::Amp];


        // i.e. which one is the x-axis of our "3D" view panels
        // why does this need to be a method as opposed to property? It's overridden somewhere? GraphicXyzRasterizers?
        int currentlyVisibleRYBDim = getPrimaryViewDimension();

        // sometimes we don't need to calculate depth dims, i.e. for 2D waveform views
        midIcpt.cube = cube;
        midIcpt.adjustedX = midIcpt.x;
        pathDeformer->adjustControlPoints(midIcpt, config);

        for (int i = 0; i < dims.numHidden(); ++i) {
            Vertex2 midCopy(midIcpt.adjustedX, midIcpt.y);

            // hidden dims are those interpolation dims (red/yellow/blue) which are NOT the primary view dimension,
            // e.g. if yellow is the 3D panels' x-axis, red/blue are the hidden dims
            int hiddenDim = dims.hidden[i];

            for (int j = 0; j < numElementsInArray(independentDims); ++j) {
                poleA[j] = hiddenDim == independentDims[j] ? 0.f : morph[independentDims[j]];
                poleB[j] = hiddenDim == independentDims[j] ? 1.f : morph[independentDims[j]];
            }

            // put the morph cursor at the corners of the cube because in this operation we want to visualize
            // the edges of the Trilinear Cube -- depth verts are either ends of these edges
            MorphPosition posA(poleA[0], poleA[1], poleA[2]);
            cube->getFinalIntercept(reduct, posA);
            Intercept beforeIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
            beforeIcpt.adjustedX = beforeIcpt.x;

            // legacy code mutates the adjustedX property -- can re reliably factor that out to a later step in PathDeformingPositioner?
            // maybe we just need to take such a PointPositioner as argument
            pathDeformer->adjustControlPoints(beforeIcpt, posA, hiddenDim == currentlyVisibleRYBDim, config);

            MorphPosition posB(poleB[0], poleB[1], poleB[2]);
            cube->getFinalIntercept(reduct, posB);
            Intercept afterIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
            afterIcpt.adjustedX = afterIcpt.x;

            //
            pathDeformer->adjustControlPoints(afterIcpt, posB, hiddenDim == currentlyVisibleRYBDim, config);

            Vertex2 before(beforeIcpt.adjustedX, beforeIcpt.y);
            Vertex2 after(afterIcpt.adjustedX, afterIcpt.y);

            if (cyclic) {
                if ((before.x > 1) != (after.x > 1)) {
                    Vertex2 before2 = before;
                    Vertex2 after2    = after;
                    Vertex2 mid2     = midCopy;

                    after2.x  -= 1.f;
                    before2.x -= 1.f;
                    mid2.x -= 1.f;

                    colorPoints.emplace_back(cube, before2, mid2, after2, hiddenDim);
                }

                if (before.x > 1 && after.x > 1) {
                    after.x   -= 1.f;
                    before.x  -= 1.f;
                    midCopy.x -= 1.f;
                }
            }

            colorPoints.emplace_back(cube, before, midCopy, after, hiddenDim);
        }
    }

    return colorPoints;
}
