#pragma once

#include <set>
#include "../App/SingletonAccessor.h"
#include "../Array/ScopedAlloc.h"
#include "../Inter/PanelState.h"
#include "../Inter/InteractorListener.h"
#include "Mesh.h"
#include "Interpolator/Trilinear/TrilinearCube.h"

using std::set;

class Mesh;

class CollisionDetector :
        public SingletonAccessor
    ,   public InteractorListener {
public:
    enum CollisionDim { Time, Key, Mod, Phase, nDims };

    CollisionDetector(SingletonRepo* repo, CollisionDim nonIntersectingDim);
    ~CollisionDetector() override = default;

    void setCurrentSelection(Mesh* mesh, Vertex* vertex);
    void setCurrentSelection(Mesh* mesh, TrilinearCube* lineCube);
    void setCurrentSelection(Mesh* mesh, const Array<Vertex*>& vertex);
    void setCurrentSelection(Mesh* mesh, const vector<Vertex*>& vertex);
    void setCurrentSelection(Mesh* mesh, const vector<VertexFrame>& frames);
    void selectionChanged(Mesh* mesh, const vector<VertexFrame>& frames) override;

    void update(Mesh* mesh);
    bool validate();

    void setNonintersectingDimension(CollisionDim dim);
    void setDimSlices(int slices)   { dimSlices = slices; sliceSpacing = 1 / float(dimSlices); }
    void setEnabled(bool is)        { enabled = is; }

private:
    static const int numIndepDims = 3;
    static const int nVerts = TrilinearCube::numVerts;

    CollisionDim nonIntersectingDim;
    bool timeIsApplicable;
    bool enabled;
    int dimSlices;
    int numMeshCubes;
    float sliceSpacing;

    // storage for partial results
    int dims[nDims];
    float minV[nDims];
    float maxV[nDims];
    float a[nDims];
    float b[nDims];
    float c[nDims];
    float d[nDims];
    float sliceSpacings[numIndepDims];
    float selectMinima[numIndepDims];
    float selectMaxima[numIndepDims];

    set<TrilinearCube*> selectedLines;

    ScopedAlloc<Float32> meshVerts;
    ScopedAlloc<Float32> selectedVerts;

    vector<TrilinearCube*> meshAddrsA;
    vector<TrilinearCube*> meshAddrsB;

    /* ----------------------------------------------------------------------------- */

    static void addLinesToSet(set<TrilinearCube*>& selected, Vertex* vert) {
        for (auto& owner : vert->owners) {
            if(owner != nullptr) {
                selected.insert(owner);
            }
        }
    }

    bool validate(Mesh* mesh, const std::set<TrilinearCube*>& vertex);
};
