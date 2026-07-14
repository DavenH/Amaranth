#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <Design/Updating/Updater.h>

#include "E3Rasterizer.h"
#include <Inter/EnvelopeInter2D.h>
#include <Inter/EnvelopeInter3D.h>
#include <UI/VertexPanels/Waveform3D.h>
#include <UI/VisualDsp.h>
#include <Util/CycleEnums.h>

E3Rasterizer::E3Rasterizer(SingletonRepo* repo)	:
        SingletonAccessor(repo, "E3Rasterizer")
    ,   Rasterization::EnvelopeGridRasterizer() {
}

void E3Rasterizer::init() {
}

int E3Rasterizer::getIncrement() {
    return getObj(EnvelopeInter3D).isDetailReduced() ? 6 : 1;
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

    int res = getConstant(EnvResolution);

    ResizeParams params(gridSize, &columnArray, &columns, nullptr, nullptr, nullptr, nullptr, nullptr);
    params.setExtraParams(res, -1, -1, true);

    ScopedLock sl(arrayLock);
    Mesh* currentMesh = getObj(MeshLibrary).getCurrentMesh(getObj(EnvelopeInter2D).layerType);
    setMesh(currentMesh);
    getObj(VisualDsp).resizeArrays(params);
    int dependentAxis = getSetting(CurrentMorphAxis);
    getRequest().primaryViewDimension = dependentAxis;
    renderGrid(currentMesh, columns, res, dependentAxis);
}
