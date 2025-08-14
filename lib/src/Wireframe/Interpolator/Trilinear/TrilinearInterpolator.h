#pragma once

#include "TrilinearCube.h"
#include "../MeshInterpolator.h"
#include "MorphPosition.h"

using std::vector;

template<
    typename MeshType,
    typename = std::enable_if_t<std::is_base_of_v<Mesh, MeshType>>>
class TrilinearInterpolator : public MeshInterpolator<MeshType*> {
public:
    explicit TrilinearInterpolator(MeshType* mesh);
    ~TrilinearInterpolator() override = default;

    vector<Intercept> interpolate(MeshType* usedMesh) override {
        if (!usedMesh || usedMesh->getNumCubes() == 0) {
            return {};
        }

        vector<Intercept> controlPoints;
        preCleanup();

        int zDim = overrideDim ? overridingDim : getPrimaryViewDimension();

        float independent = zDim == Vertex::Time ? morph.time :
                            zDim == Vertex::Red ?  morph.red  : morph.blue;

        float poleA[3];
        float poleB[3];
        int indDims[3] = { Vertex::Time, Vertex::Red, Vertex::Blue };

        Vertex middle;
        Intercept midIcpt;

        for (auto cube : usedMesh->getCubes()) {
            cube->getInterceptsFast(zDim, reduct, morph);

            if (reduct.pointOverlaps) {
                Vertex* a = &reduct.v0;
                Vertex* b = &reduct.v1;
                Vertex* vertex = &reduct.v;

                if (calcDepthDims) {
                    TrilinearCube::vertexAt(independent, zDim, a, b, vertex);

                    midIcpt.x = vertex->values[Vertex::Phase] + oscPhase;
                    midIcpt.y = vertex->values[Vertex::Amp];
                }

                wrapVertices(a->values[zDim], a->values[Vertex::Phase],
                             b->values[zDim], b->values[Vertex::Phase],
                             independent);

                TrilinearCube::vertexAt(independent, zDim, a, b, vertex);

                float x = vertex->values[Vertex::Phase] + oscPhase;

                if (cyclic) {
                    while (x >= 1.f) x -= 1.f;
                    while (x < 0.f) x += 1.f;

                    jassert(x >= 0.f && x < 1.f);
                    jassert(xMaximum == 1.f && xMinimum == 0.f);
                } else {
                    NumberUtils::constrain(x, xMinimum, xMaximum);
                }

                Intercept intercept(x, vertex->values[Vertex::Amp], cube);

                intercept.shp = vertex->values[Vertex::Curve];
                intercept.adjustedX = intercept.x;

                jassert(intercept.y == intercept.y);
                jassert(intercept.x == intercept.x);

                // can be NaN, short circuit here so it doesn't propagate
                if(!(intercept.y == intercept.y))
                    intercept.y = 0.5f;

                switch (scalingType) {
                    case Unipolar:
                        break;

                    case Bipolar:
                        intercept.y = 2 * intercept.y - 1;
                        break;

                    case HalfBipolar:
                        intercept.y -= 0.5f;
                        break;

                    default: break;
                }

                applyPaths(intercept, morph);
                controlPoints.emplace_back(intercept);

                int currentlyVisibleRYBDim = getPrimaryViewDimension();
                if (calcDepthDims) {
                    midIcpt.cube = cube;
                    midIcpt.adjustedX = midIcpt.x;
                    applyPaths(midIcpt, morph);

                    for (int i = 0; i < dims.numHidden(); ++i) {
                        Vertex2 midCopy(midIcpt.adjustedX, midIcpt.y);

                        int hiddenDim = dims.hidden[i];

                        for (int j = 0; j < numElementsInArray(indDims); ++j) {
                            poleA[j] = hiddenDim == indDims[j] ? 0.f : morph[indDims[j]];
                            poleB[j] = hiddenDim == indDims[j] ? 1.f : morph[indDims[j]];
                        }

                        MorphPosition posA(poleA[0], poleA[1], poleA[2]);
                        cube->getFinalIntercept(reduct, posA);
                        Intercept beforeIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
                        beforeIcpt.adjustedX = beforeIcpt.x;
                        applyPaths(beforeIcpt, posA, hiddenDim == currentlyVisibleRYBDim);

                        MorphPosition posB(poleB[0], poleB[1], poleB[2]);
                        cube->getFinalIntercept(reduct, posB);
                        Intercept afterIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
                        afterIcpt.adjustedX = afterIcpt.x;
                        applyPaths(afterIcpt, posB, hiddenDim == currentlyVisibleRYBDim);

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
            }
        }

        // sorts by x-value, not adjusted x,
        std::sort(controlPoints.begin(), controlPoints.end());

        return controlPoints;
    }

    void setMorphPosition(const MorphPosition& position) { morph = position; }

    void setYellow(float yellow)   { morph.time = yellow;   }
    void setBlue(float blue)       { morph.blue = blue;     }
    virtual void setRed(float red) { morph.red  = red;      }

protected:
    MeshType* mesh;
    MorphPosition morph;
    vector<TrilinearCube*> vertices;
};