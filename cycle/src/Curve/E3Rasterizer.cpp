#include <App/MeshLibrary.h>
#include <Curve/EnvelopeMesh.h>

#include "E3Rasterizer.h"

#include <Design/Updating/Updater.h>

#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../UI/VertexPanels/Envelope3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"


E3Rasterizer::E3Rasterizer(SingletonRepo* repo)
	: 	MeshRasterizer(repo, "E3Rasterizer") {
	lowResCurves 	= true;
	cyclic 			= false;
	calcDepthDims 	= false;
	scalingType		= MeshRasterizer::HalfBipolar;
	xMaximum 		= 10.f;
}


E3Rasterizer::~E3Rasterizer() {
}


void E3Rasterizer::init() {
}


int E3Rasterizer::getIncrement() {
    return getObj(EnvelopeInter3D).isDetailReduced() ? 6 : 1;
}


float &E3Rasterizer::getPrimaryDimensionVar() {
    int dim = getSetting(CurrentMorphAxis);
    jassert(dim != Vertex::Time);

	return morph[dim];
}


void E3Rasterizer::performUpdate(int updateType) {
    if (updateType != UpdateType::Update)
        return;

    int increment = getIncrement();
    int numPixels = getObj(Waveform3D).getWindowWidthPixels();
    int gridSize = numPixels / increment;

	float& indie = getPrimaryDimensionVar();

    if (numPixels <= 0) {
        ScopedLock sl(arrayLock);
		columnArray.clear();
		columns.clear();

		return;
	}

	ScopedAlloc<Ipp32f> zoomProgress(gridSize);
	zoomProgress.ramp();

	int res = getConstant(EnvResolution);
	float invCol = 1 / float(res);
	float invGrid = 1 / float(gridSize - 1);

	ResizeParams params(gridSize, &columnArray, &columns, 0, 0, 0, 0, 0);
	params.setExtraParams(res, -1, -1, true);

	ScopedLock sl(arrayLock);
	mesh = getObj(MeshLibrary).getCurrentMesh(getObj(EnvelopeInter2D).layerType);
	getObj(VisualDsp).resizeArrays(params);

    for (int colIdx = 0; colIdx < gridSize; ++colIdx) {
        Column& col = columns[colIdx];

		indie = colIdx * invGrid;
		calcCrossPoints(mesh);

		if (isSampleable())
			sampleWithInterval(col, invCol, 0.f);
		else
			col.zero();
	}
}
