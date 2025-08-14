#include <App/MeshLibrary.h>
#include <Design/Updating/Updater.h>

#include "E3Rasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

E3Rasterizer::E3Rasterizer(SingletonRepo* repo)	:
        SingletonAccessor(repo, "E3Rasterizer")
    ,	OldMeshRasterizer("E3Rasterizer") {
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

int E3Rasterizer::getPrimaryViewDimension() {
    return getSetting(CurrentMorphAxis);
}

void E3Rasterizer::performUpdate(UpdateType updateType) {
    if (updateType != Update) {
        return;
    }

    int increment = getIncrement();
    int numPixels = getObj(Waveform3D).getWindowWidthPixels();
    int gridSize = numPixels / increment;

    if (numPixels <= 0) {
        ScopedLock sl(arrayLock);
        columnArray.clear();
        columns.clear();

        return;
    }

    ScopedAlloc<Float32> zoomProgress(gridSize);
    zoomProgress.ramp();

    int res = getConstant(EnvResolution);
    float invCol = 1 / float(res);
    float invGrid = 1 / float(gridSize - 1);

    ResizeParams params(gridSize, &columnArray, &columns, nullptr, nullptr, nullptr, nullptr, nullptr);
    params.setExtraParams(res, -1, -1, true);

    ScopedLock sl(arrayLock);
    mesh = getObj(MeshLibrary).getCurrentMesh(getObj(EnvelopeInter2D).layerType);
    getObj(VisualDsp).resizeArrays(params);
    int dependentAxis = getSetting(CurrentMorphAxis);

    for (int colIdx = 0; colIdx < gridSize; ++colIdx) {
        Column& col = columns[colIdx];

        MorphPosition& p = getMorphPosition();
        p[dependentAxis].setValueDirect(colIdx * invGrid);
        generateControlPoints(mesh, 0.f);

        if (isSampleable()) {
            sampleWithInterval(col, invCol, 0.f);
        } else {
            col.zero();
        }
    }
}
