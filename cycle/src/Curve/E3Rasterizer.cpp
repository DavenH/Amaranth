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
    auto& request = rasterizer.getRequest();
    request.lowResCurves = true;
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::PointScalingMode::HalfBipolar;
    request.xMinimum = 0.f;
    request.xMaximum = 10.f;
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
    rasterizer.getRequest().primaryViewDimension = dependentAxis;

    for (int colIdx = 0; colIdx < gridSize; ++colIdx) {
        Column& col = columns[colIdx];

        MorphPosition& p = getMorphPosition();
        p[dependentAxis].setValueDirect(colIdx * invGrid);
        renderMesh(currentMesh);

        if (isSampleable()) {
            rasterizer.samplerView().samplePerfectly(invCol, col, 0.f);
        } else {
            col.zero();
        }
    }
}

void E3Rasterizer::cleanUp() {
    rasterizer.clean();

    ScopedLock sl(rasterizerData.lock);
    rasterizerData.zeroIndex = 0;
    rasterizerData.oneIndex = 0;
    rasterizerData.buffer.clear();
    rasterizerData.waveX.nullify();
    rasterizerData.waveY.nullify();
    rasterizerData.colorPoints.clear();
    rasterizerData.intercepts.clear();
    rasterizerData.curves.clear();
}

bool E3Rasterizer::hasEnoughCubesForCrossSection() {
    return mesh != nullptr && mesh->hasEnoughCubesForCrossSection();
}

bool E3Rasterizer::isSampleable() const {
    return rasterizer.samplerView().isSampleable();
}

bool E3Rasterizer::isSampleableAt(float x) const {
    return rasterizer.samplerView().isSampleableAt(x);
}

float E3Rasterizer::sampleAt(double angle) {
    return rasterizer.samplerView().sampleAt(angle);
}

float E3Rasterizer::sampleAt(double angle, int& currentIndex) {
    return rasterizer.samplerView().sampleAt(angle, currentIndex);
}

float E3Rasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return rasterizer.samplerView().samplePerfectly(delta, buffer, phase);
}

int E3Rasterizer::getPaddingSize() const {
    return rasterizer.getPaddingSize();
}

void E3Rasterizer::renderMesh(Mesh* mesh) {
    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    rasterizer.render(mesh);
    publishSnapshot();
}

void E3Rasterizer::publishSnapshot() {
    Rasterization::RasterizerSnapshotBuilder().publish(rasterizerData, rasterizer.createSnapshotSource());
}
