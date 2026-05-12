#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <Curve/EnvelopeMesh.h>
#include <Design/Updating/Updater.h>

#include "E3Rasterizer.h"
#include "../Inter/EnvelopeInter2D.h"
#include "../Inter/EnvelopeInter3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/VisualDsp.h"
#include "../Util/CycleEnums.h"

E3Rasterizer::E3Rasterizer(SingletonRepo* repo)	:
        SingletonAccessor(repo, "E3Rasterizer") {
    auto& request = getRequest();
    request.lowResCurves = true;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::PointScalingMode::HalfBipolar;
    request.xMinimum = 0.f;
    request.xMaximum = 10.f;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = request.cyclic;
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

    ScopedAlloc<Float32> zoomProgress(gridSize);
    zoomProgress.ramp();

    int res = getConstant(EnvResolution);
    float invCol = 1 / float(res);
    float invGrid = 1 / float(gridSize - 1);

    ResizeParams params(gridSize, &columnArray, &columns, nullptr, nullptr, nullptr, nullptr, nullptr);
    params.setExtraParams(res, -1, -1, true);

    ScopedLock sl(arrayLock);
    Mesh* currentMesh = getObj(MeshLibrary).getCurrentMesh(getObj(EnvelopeInter2D).layerType);
    setMesh(currentMesh);
    getObj(VisualDsp).resizeArrays(params);
    int dependentAxis = getSetting(CurrentMorphAxis);
    getRequest().primaryViewDimension = dependentAxis;

    for (int colIdx = 0; colIdx < gridSize; ++colIdx) {
        Column& col = columns[colIdx];

        MorphPosition& p = getMorphPosition();
        p[dependentAxis].setValueDirect(colIdx * invGrid);
        renderMesh(currentMesh);

        auto sampler = samplerView();
        if (sampler.isSampleable()) {
            sampler.samplePerfectly(invCol, col, 0.f);
        } else {
            col.zero();
        }
    }
}

void E3Rasterizer::cleanUp() {
    cleanTrilinearRasterization();
}

void E3Rasterizer::renderMesh(Mesh* mesh) {
    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    setMesh(mesh);
    renderTrilinearWaveform(0.f);
    publishTrilinearSnapshot();
}
