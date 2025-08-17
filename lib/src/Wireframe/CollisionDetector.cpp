#include "CollisionDetector.h"

#include <Array/VecOps.h>

#include "Mesh.h"
#include "Interpolator/Trilinear/TrilinearVertex.h"
#include "../App/SingletonRepo.h"
#include "../Array/ScopedAlloc.h"
#include "../Util/Geometry.h"

CollisionDetector::CollisionDetector(SingletonRepo* repo, CollisionDim nonIntersectingDim) :
        SingletonAccessor   (repo, "CollisionDetector")
    ,    nonIntersectingDim (nonIntersectingDim)
    ,    timeIsApplicable   (nonIntersectingDim != Key)
    ,    dimSlices          (11)
    ,    enabled            (true)
    ,    numMeshCubes       (0) {
    sliceSpacing = 1 / float(dimSlices);

    dims[Time]  = Time;
    dims[Key]   = Key;
    dims[Mod]   = Mod;
    dims[Phase] = Phase;
}

void CollisionDetector::update(Mesh* mesh) {
    if(! enabled) {
        return;
    }

    numMeshCubes = mesh->getNumCubes();
    int numSelectedLines = selectedLines.size();

    vector<TrilinearCube*> uniqueMeshCubes;
    uniqueMeshCubes.reserve(numMeshCubes);

    for (auto& it : mesh->getCubes()) {
        if(selectedLines.find(it) != selectedLines.end()) {
            uniqueMeshCubes.push_back(it);
        }
    }

    // nothing to collide with
    if(numSelectedLines == 0 || uniqueMeshCubes.size() <= 1) {
        return;
    }

    Vertex* currVert;

    meshVerts.resize(uniqueMeshCubes.size() * nVerts * nDims);
    selectedVerts.resize(numSelectedLines * nVerts * nDims);

    int index = 0;

    for (int i = 0; i < numIndepDims; ++i) {
        selectMinima[i] = 1;
        selectMaxima[i] = 0;
    }

    for (auto cube : selectedLines) {
        int baseIndex;

        for (int i = 0; i < nVerts; ++i) {
            currVert = cube->getVertex(i);

            baseIndex = index * nVerts * nDims + i * nDims;
            selectedVerts[baseIndex + Time ] = currVert->values[Vertex::Time];
            selectedVerts[baseIndex + Key  ] = currVert->values[Vertex::Red];
            selectedVerts[baseIndex + Mod  ] = currVert->values[Vertex::Blue];
            selectedVerts[baseIndex + Phase] = currVert->values[Vertex::Phase];

            selectMinima[0] = jmin(selectMinima[0], selectedVerts[baseIndex + 0]);
            selectMinima[1] = jmin(selectMinima[1], selectedVerts[baseIndex + 1]);
            selectMinima[2] = jmin(selectMinima[2], selectedVerts[baseIndex + 2]);

            selectMaxima[0] = jmax(selectMaxima[0], selectedVerts[baseIndex + 0]);
            selectMaxima[1] = jmax(selectMaxima[1], selectedVerts[baseIndex + 1]);
            selectMaxima[2] = jmax(selectMaxima[2], selectedVerts[baseIndex + 2]);
        }

        ++index;
    }

    for(int i = 0; i < numIndepDims; ++i) {
        sliceSpacings[i] = sliceSpacing * (selectMaxima[i] - selectMinima[i]);
    }

    int lineIdx = 0;
    for (auto cube : uniqueMeshCubes) {
        int baseIndex;
        for (int i = 0; i < nVerts; ++i) {
            currVert = cube->getVertex(i);

            baseIndex = lineIdx * nVerts * nDims + i * nDims;
            meshVerts[baseIndex + Time ] = currVert->values[Vertex::Time];
            meshVerts[baseIndex + Key  ] = currVert->values[Vertex::Red];
            meshVerts[baseIndex + Mod  ] = currVert->values[Vertex::Blue];
            meshVerts[baseIndex + Phase] = currVert->values[Vertex::Phase];
        }

        // todo
        // create phantom verts where the line splices

        ++lineIdx;
    }
}

void CollisionDetector::setCurrentSelection(Mesh* mesh, Vertex* vertex) {
    selectedLines.clear();
    addLinesToSet(selectedLines, vertex);

    update(mesh);
}

void CollisionDetector::setCurrentSelection(Mesh* mesh, TrilinearCube* lineCube) {
    selectedLines.clear();
    selectedLines.insert(lineCube);

    update(mesh);
}

void CollisionDetector::setCurrentSelection(Mesh* mesh, const vector<Vertex*>& verts) {
    selectedLines.clear();

    for (auto vert : verts)
        addLinesToSet(selectedLines, vert);

    update(mesh);
}

void CollisionDetector::setCurrentSelection(Mesh* mesh, const Array<Vertex*>& verts) {
    selectedLines.clear();

    for (auto vert : verts) {
        addLinesToSet(selectedLines, vert);
    }

    update(mesh);
}

void CollisionDetector::setCurrentSelection(Mesh* mesh, const vector<VertexFrame>& frames) {
    selectedLines.clear();

    for (const auto& frame : frames) {
        addLinesToSet(selectedLines, frame.vert);
    }

    update(mesh);
}

void CollisionDetector::setNonintersectingDimension(CollisionDim dim) {
    nonIntersectingDim = dim;
    timeIsApplicable = nonIntersectingDim != Key;
}

//#define CD_VERBOSE

/**
 * Checks for overlapping lines in the phase vs (time, key, mod) dimension pairs by
 * iteratively checking divisions of the two non-primary dimensions
 */
bool CollisionDetector::validate() {
    if (!enabled) {
        return true;
    }

    bool meshLineIsInSlice, selectedLineIsInSlice;
    float axes[numIndepDims];

  #ifdef CD_VERBOSE
    dout << "Collision Detection debug" << "\n";
    dout << "___________" << "\n";
    dout << "All lines size: " << numMeshLines << "\n";
    dout << "Selected lines size: " << numSelectedLines << "\n";
    dout << "\n";
  #endif

    bool sameLines    = true;
    int dimAfter      = 0;
    int dim2After     = 0;
    int baseIndexL    = 0;
    int baseIndexM    = 0;
    int numDim2Slices = timeIsApplicable ? dimSlices : 1;
    int dimIdx        = nonIntersectingDim;

//    for(int dimIdx = 0; dimIdx < numIndepDims; ++dimIdx)
//    {
    axes[dimIdx] = 0; //???

    for (int l = 0; l < numMeshCubes; ++l) {
        for (int m = 0; m < (int) selectedLines.size(); ++m) {
            baseIndexL = l * nVerts * nDims;
            baseIndexM = m * nVerts * nDims;

            for (int dim1SliceIdx = 0; dim1SliceIdx < dimSlices; ++dim1SliceIdx) {
                dimAfter = dims[(dimIdx + 1) % numIndepDims];
                axes[dimAfter] = selectMinima[dimAfter] + dim1SliceIdx * sliceSpacings[dimAfter];

                for (int dim2SliceIdx = 0; dim2SliceIdx < numDim2Slices; ++dim2SliceIdx) {
                    dim2After         = dims[(dimIdx + 2) % numIndepDims];
                    axes[dim2After] = timeIsApplicable ? selectMinima[dim2After] + dim2SliceIdx * sliceSpacings[dim2After] : 0;

                    float xDimValue = axes[dimAfter];
                    float yDimValue = axes[dim2After];
                    float x1, x2, y1, y2;

                    float nnx = meshVerts[baseIndexL + nDims * 0 + dimAfter];
                    float nny = meshVerts[baseIndexL + nDims * 0 + dim2After];
                    float xxx = meshVerts[baseIndexL + nDims * 3 + dimAfter];
                    float xxy = meshVerts[baseIndexL + nDims * 3 + dim2After];

                    if(nnx < xxx)    { x1 = nnx; x2 = xxx; }
                    else             { x1 = xxx; x2 = nnx; }

                    if(nny < xxy)    { y1 = nny; y2 = xxy; }
                    else             { y1 = xxy; y2 = nny; }

                    if(x2 == 1.f)    { x2 += 0.000001f;    }
                    if(y2 == 1.f)    { y2 += 0.000001f;    }

                    meshLineIsInSlice = (xDimValue >= x1 && xDimValue < x2);
                    meshLineIsInSlice &= ! timeIsApplicable || (yDimValue >= y1 && yDimValue < y2);

                    if(! meshLineIsInSlice) {
                        continue;
                    }

                    nnx = meshVerts[baseIndexL + nDims * 4 + dimAfter];
                    nny = meshVerts[baseIndexL + nDims * 4 + dim2After];
                    xxx = meshVerts[baseIndexL + nDims * 7 + dimAfter];
                    xxy = meshVerts[baseIndexL + nDims * 7 + dim2After];

                    if(nnx < xxx)    { x1 = nnx; x2 = xxx; }
                    else             { x1 = xxx; x2 = nnx; }

                    if(nny < xxy)    { y1 = nny; y2 = xxy; }
                    else             { y1 = xxy; y2 = nny; }

                    if(x2 == 1.f)    { x2 += 0.000001f;    }
                    if(y2 == 1.f)    { y2 += 0.000001f;    }

                    meshLineIsInSlice = (xDimValue >= x1 && xDimValue < x2);
                    meshLineIsInSlice &= ! timeIsApplicable || (yDimValue >= y1 && yDimValue < y2);

                    if(! meshLineIsInSlice) {
                        continue;
                    }

                    nnx = selectedVerts[baseIndexM + nDims * 0 + dimAfter];
                    nny = selectedVerts[baseIndexM + nDims * 0 + dim2After];
                    xxx = selectedVerts[baseIndexM + nDims * 3 + dimAfter];
                    xxy = selectedVerts[baseIndexM + nDims * 3 + dim2After];

                    if(nnx < xxx)    { x1 = nnx; x2 = xxx; }
                    else             { x1 = xxx; x2 = nnx; }

                    if(nny < xxy)    { y1 = nny; y2 = xxy; }
                    else             { y1 = xxy; y2 = nny; }

                    if(x2 == 1.f)    { x2 += 0.000001f;    }
                    if(y2 == 1.f)    { y2 += 0.000001f;    }

                    selectedLineIsInSlice = (xDimValue >= x1 && xDimValue < x2);
                    selectedLineIsInSlice &= ! timeIsApplicable || (yDimValue >= y1 && yDimValue < y2);

                    if(! selectedLineIsInSlice) {
                        continue;
                    }

                    nnx = selectedVerts[baseIndexM + nDims * 4 + dimAfter];
                    nny = selectedVerts[baseIndexM + nDims * 4 + dim2After];
                    xxx = selectedVerts[baseIndexM + nDims * 7 + dimAfter];
                    xxy = selectedVerts[baseIndexM + nDims * 7 + dim2After];

                    if(nnx < xxx)    { x1 = nnx; x2 = xxx; }
                    else             { x1 = xxx; x2 = nnx; }

                    if(nny < xxy)    { y1 = nny; y2 = xxy; }
                    else             { y1 = xxy; y2 = nny; }

                    if(x2 == 1.f)    { x2 += 0.000001f;    }
                    if(y2 == 1.f)    { y2 += 0.000001f;    }

                    selectedLineIsInSlice = (xDimValue >= x1 && xDimValue < x2);
                    selectedLineIsInSlice &= ! timeIsApplicable || (yDimValue >= y1 && yDimValue < y2);

                    if(! selectedLineIsInSlice) {
                        continue;
                    }

                    if (timeIsApplicable) {
                        // TODO DH 2025:I don't remember why we have this inlined, for speed? We have these same calculations in TrilinearVertex / TrilinearCube?

                        // lerp the 4 pairs along first dimension
                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 0], 1 - xDimValue, minV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 1],     xDimValue, minV, 4);
                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 2], 1 - xDimValue, maxV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 3],     xDimValue, maxV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 0], 1 - xDimValue, minV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 1],     xDimValue, minV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 2], 1 - xDimValue, maxV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 3],     xDimValue, maxV, 4);

                        VecOps::mul    (minV, 1 - yDimValue, a, 4);
                        VecOps::addProd(maxV,     yDimValue, a, 4);
                        // ippsMulC_32f        (minV, 1 - yDimValue, a, 4);
                        // ippsAddProductC_32f (maxV,     yDimValue, a, 4);

                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 4], 1 - xDimValue, minV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 5],     xDimValue, minV, 4);
                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 6], 1 - xDimValue, maxV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 7],     xDimValue, maxV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 4], 1 - xDimValue, minV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 5],     xDimValue, minV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 6], 1 - xDimValue, maxV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 7],     xDimValue, maxV, 4);

                        VecOps::mul    (minV, 1 - yDimValue, b, 4);
                        VecOps::addProd(maxV,     yDimValue, b, 4);
                        // ippsMulC_32f        (minV, 1 - yDimValue, b, 4);
                        // ippsAddProductC_32f (maxV,     yDimValue, b, 4);

                        VecOps::mul    (&meshVerts[baseIndexM + nDims * 0], 1 - xDimValue, minV, 4);
                        VecOps::addProd(&meshVerts[baseIndexM + nDims * 1],     xDimValue, minV, 4);
                        VecOps::mul    (&meshVerts[baseIndexM + nDims * 2], 1 - xDimValue, maxV, 4);
                        VecOps::addProd(&meshVerts[baseIndexM + nDims * 3],     xDimValue, maxV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexM + nDims * 0], 1 - xDimValue, minV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexM + nDims * 1],     xDimValue, minV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexM + nDims * 2], 1 - xDimValue, maxV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexM + nDims * 3],     xDimValue, maxV, 4);

                        VecOps::mul    (minV, 1 - yDimValue, c, 4);
                        VecOps::addProd(maxV,     yDimValue, c, 4);
                        // ippsMulC_32f        (minV, 1 - yDimValue, c, 4);
                        // ippsAddProductC_32f (maxV,     yDimValue, c, 4);

                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 4], 1 - xDimValue, minV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 5],     xDimValue, minV, 4);
                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 6], 1 - xDimValue, minV, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 7],     xDimValue, minV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexM + nDims * 4], 1 - xDimValue, minV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexM + nDims * 5],     xDimValue, minV, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexM + nDims * 6], 1 - xDimValue, maxV, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexM + nDims * 7],     xDimValue, maxV, 4);

                        VecOps::mul    (minV, 1 - yDimValue, d, 4);
                        VecOps::addProd(maxV,     yDimValue, d, 4);
                        // ippsMulC_32f        (minV, 1 - yDimValue, d, 4);
                        // ippsAddProductC_32f (maxV,     yDimValue, d, 4);
                    } else {
                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 0], 1 - xDimValue, a, 4);
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 1],     xDimValue, a, 4);
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 0], 1 - xDimValue, a, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 1],     xDimValue, a, 4);

                        VecOps::mul    (&meshVerts[baseIndexL + nDims * 2], 1 - xDimValue, b, 4);  // changed * 0 to * 2
                        VecOps::addProd(&meshVerts[baseIndexL + nDims * 3],     xDimValue, b, 4);  // changed * 0 to * 3
                        // ippsMulC_32f        (&meshVerts[baseIndexL + nDims * 0], 1 - xDimValue, b, 4);
                        // ippsAddProductC_32f (&meshVerts[baseIndexL + nDims * 1],     xDimValue, b, 4);

                        VecOps::mul    (&meshVerts[baseIndexM + nDims * 0], 1 - xDimValue, c, 4);
                        VecOps::addProd(&meshVerts[baseIndexM + nDims * 1],     xDimValue, c, 4);
                        // ippsMulC_32f        (&selectedVerts[baseIndexM + nDims * 0], 1 - xDimValue, c, 4);
                        // ippsAddProductC_32f (&selectedVerts[baseIndexM + nDims * 1],     xDimValue, c, 4);

                        VecOps::mul    (&meshVerts[baseIndexM + nDims * 2], 1 - xDimValue, d, 4);
                        VecOps::addProd(&meshVerts[baseIndexM + nDims * 3],     xDimValue, d, 4);
                        // ippsMulC_32f        (&selectedVerts[baseIndexM + nDims * 2], 1 - xDimValue, d, 4);
                        // ippsAddProductC_32f (&selectedVerts[baseIndexM + nDims * 3],     xDimValue, d, 4);
                    }

                    int isctMode = Geometry::doLineSegmentsIntersect(
                            a[dimIdx], b[dimIdx], c[dimIdx], d[dimIdx],
                            a[Phase], b[Phase], c[Phase], d[Phase]);

                    if (isctMode == Geometry::Intersect) {
                      #ifdef CD_VERBOSE
                        dout << "Lines " << l << " and " << m << " have overlapping dimensions" << "\n";
                        dout << "(" << a[dimIdx] << "," << a[Phase] << ")(" << b[dimIdx] << "," << b[Phase] << ") || ("
                             << c[dimIdx] << ", " << c[Phase] << ")(" << d[dimIdx] << "," << d[Phase] << ")" << "\n" << "\n";
                        dout << "\n";
                      #endif

                        return false;
                    } else if (isctMode == Geometry::IntersectAtEnds) {
                      #ifdef CD_VERBOSE
                        dout << "Lines " << l << " and " << m << " intersect at ends" << "\n";
                        dout << "(" << a[dimIdx] << "," << a[Phase] << ")(" << b[dimIdx] << "," << b[Phase] << ") || ("
                             << c[dimIdx] << ", " << c[Phase] << ")(" << d[dimIdx] << "," << d[Phase] << ")" << "\n" << "\n";
                      #endif

                        return false;
                    } else {
                      #ifdef CD_VERBOSE
                        dout << "Lines " << l << " and " << m << " do not intersect at " << axes[dimAfter] << ", " << axes[dim2After] << "\n";
                        dout << "(" << a[dimIdx] << "," << a[Phase] << ")(" << b[dimIdx] << "," << b[Phase] << ") || ("
                             << c[dimIdx] << ", " << c[Phase] << ")(" << d[dimIdx] << "," << d[Phase] << ")" << "\n" << "\n";
                      #endif
                    }
                }
            }
        }
    }

    return true;
}

void CollisionDetector::selectionChanged(Mesh* mesh, const vector<VertexFrame>& frames) {
    setCurrentSelection(mesh, frames);
}
