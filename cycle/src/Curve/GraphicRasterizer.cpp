#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>
#include <Inter/Interactor.h>

#include "GraphicRasterizer.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/VisualDsp.h"

namespace {
constexpr int kV2GraphicMaxIntercepts = 128;
constexpr int kV2GraphicMaxCurves = 256;
constexpr int kV2GraphicMaxWavePoints = 4096;
}

GraphicRasterizer::GraphicRasterizer(
            SingletonRepo* repo
        ,   Interactor* interactor
        ,   const String& name
        ,   int layerGroup
        ,   bool isCyclic
        ,   float margin) : SingletonAccessor(repo, name)
    ,   MeshRasterizer(name)
    ,   layerGroup(layerGroup)
    ,   interactor(interactor) {
    cyclic = isCyclic;
    addListener(this);
    setLimits(-margin, 1 + margin);

    V2PrepareSpec prepareSpec;
    prepareSpec.capacities.maxIntercepts = kV2GraphicMaxIntercepts;
    prepareSpec.capacities.maxCurves = kV2GraphicMaxCurves;
    prepareSpec.capacities.maxWavePoints = kV2GraphicMaxWavePoints;
    prepareSpec.capacities.maxDeformRegions = 0;
    v2GraphicRasterizer.prepare(prepareSpec);
}

void GraphicRasterizer::calcCrossPoints() {
    if (! renderWithV2()) {
        MeshRasterizer::calcCrossPoints();
    }
}

void GraphicRasterizer::pullModPositionAndAdjust() {
    morph = getObj(MorphPanel).getMorphPosition();

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

    if (props->scratchChan != CommonEnums::Null) {
        morph.time = getObj(VisualDsp).getScratchPosition(props->scratchChan);
    }
}

int GraphicRasterizer::getPrimaryViewDimension() {
    return getSetting(CurrentMorphAxis);
}

bool GraphicRasterizer::renderWithV2() {
    if (mesh == nullptr || ! hasEnoughCubesForCrossSection()) {
        return false;
    }

    v2GraphicRasterizer.setMeshSnapshot(mesh);

    V2GraphicControlSnapshot controls;
    controls.morph = morph;
    controls.scaling = scalingType;
    controls.primaryDimension = getPrimaryViewDimension();
    controls.wrapPhases = cyclic;
    controls.cyclic = cyclic;
    controls.minX = xMinimum;
    controls.maxX = xMaximum;
    controls.interpolateCurves = interpolateCurves;
    controls.lowResolution = lowResCurves;
    controls.integralSampling = integralSampling;
    v2GraphicRasterizer.updateControlData(controls);

    V2GraphicArtifactsView artifacts;
    if (! v2GraphicRasterizer.extractArtifacts(artifacts)
            || artifacts.intercepts == nullptr
            || artifacts.curves == nullptr
            || artifacts.interceptCount <= 1) {
        return false;
    }

    icpts.assign(artifacts.intercepts->begin(), artifacts.intercepts->begin() + artifacts.interceptCount);
    curves.assign(artifacts.curves->begin(), artifacts.curves->begin() + artifacts.curveCount);
    colorPoints.clear();
    zeroIndex = artifacts.zeroIndex;
    oneIndex = artifacts.oneIndex;

    int wavePointCount = artifacts.wavePointCount;
    if (wavePointCount > 0) {
        updateBuffers(wavePointCount);
        artifacts.waveX.copyTo(waveX.withSize(wavePointCount));
        artifacts.waveY.copyTo(waveY.withSize(wavePointCount));

        int slopeCount = jmax(0, wavePointCount - 1);
        if (slopeCount > 0) {
            artifacts.diffX.copyTo(diffX.withSize(slopeCount));
            artifacts.slope.copyTo(slope.withSize(slopeCount));
        }
    } else {
        waveX.nullify();
        waveY.nullify();
    }

    unsampleable = wavePointCount <= 1;
    return true;
}
