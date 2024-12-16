#pragma once

#include <set>
#include <ippdefs.h>
#include "VertCube.h"
#include "Mesh.h"
#include "../App/SingletonAccessor.h"
#include "../Array/ScopedAlloc.h"
#include "../Inter/PanelState.h"
#include "../Inter/InteractorListener.h"

using std::set;

class Mesh;

class CollisionDetector :
		public SingletonAccessor
	,	public InteractorListener {
public:
	enum CollisionDim { Time, Key, Mod, Phase, nDims };

	CollisionDetector(SingletonRepo* repo, CollisionDim nonIntersectingDim);
	~CollisionDetector() override = default;

	void setCurrentSelection(Mesh* mesh, Vertex* vertex);
	void setCurrentSelection(Mesh* mesh, VertCube* lineCube);
	void setCurrentSelection(Mesh* mesh, const Array<Vertex*>& vertex);
	void setCurrentSelection(Mesh* mesh, const vector<Vertex*>& vertex);
	void setCurrentSelection(Mesh* mesh, const vector<VertexFrame>& frames);
	void selectionChanged(Mesh* mesh, const vector<VertexFrame>& frames) override;

	void update(Mesh* mesh);
	bool validate();

	void setNonintersectingDimension(CollisionDim dim);
	void setDimSlices(int slices) 	{ dimSlices = slices; sliceSpacing = 1 / float(dimSlices); }
	void setEnabled(bool is) 		{ enabled = is; }

private:
	static const int numIndepDims = 3;
	static const int nVerts = VertCube::numVerts;

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

	set<VertCube*> selectedLines;

	ScopedAlloc<Ipp32f> meshVerts;
	ScopedAlloc<Ipp32f> selectedVerts;

	vector<VertCube*> meshAddrsA;
	vector<VertCube*> meshAddrsB;

    /* ----------------------------------------------------------------------------- */

    static void addLinesToSet(set<VertCube*>& selected, Vertex* vert) {
    	for (auto& owner : vert->owners) {
			if(owner != nullptr) {
				selected.insert(owner);
			}
		}
	}

	bool validate(Mesh* mesh, const std::set<VertCube*>& vertex);
};
