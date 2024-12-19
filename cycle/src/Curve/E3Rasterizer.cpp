#include <App/MeshLibrary.h>
#include <Curve/EnvelopeMesh.h>
#include <Design/Updating/Updater.h>

#include "E3Rasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

E3Rasterizer::E3Rasterizer(SingletonRepo* repo)	:
        SingletonAccessor(repo, "E3Rasterizer")
    ,	MeshRasterizer("E3Rasterizer") {
    lowResCurves 	= true;
    cyclic 			= false;
    calcDepthDims 	= false;
    scalingType		= HalfBipolar;
    xMaximum 		= 10.f;
}

void E3Rasterizer::init() {
}

int E3Rasterizer::getIncrement() {
    return getObj(EnvelopeInter3D).isDetailReduced() ? 6 : 1;
}

float& E3Rasterizer::getPrimaryDimensionVar() {
    int dim = getSetting(CurrentMorphAxis);
    jassert(dim != Vertex::Time);

    return morph[dim];
}

void E3Rasterizer::performUpdate(int updateType) {
    if (updateType != Update) {
        return;
    }

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

    ResizeParams params(gridSize, &columnArray, &columns, nullptr, nullptr, nullptr, nullptr, nullptr);
    params.setExtraParams(res, -1, -1, true);

    ScopedLock sl(arrayLock);
    mesh = getObj(MeshLibrary).getCurrentMesh(getObj(EnvelopeInter2D).layerType);
    getObj(VisualDsp).resizeArrays(params);

    for (int colIdx = 0; colIdx < gridSize; ++colIdx) {
        Column& col = columns[colIdx];

        indie = colIdx * invGrid;
        calcCrossPoints(mesh, 0.f);

        if (isSampleable()) {
            sampleWithInterval(col, invCol, 0.f);
        } else {
            col.zero();
        }
    }
}
