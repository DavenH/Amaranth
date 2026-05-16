#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Curve/Mesh/Mesh.h>
#include <Inter/Interactor.h>
#include "GraphicRasterizer.h"
#include <UI/Panels/Morphing/MorphPanel.h>
#include <UI/VisualDsp.h>
#include <Definitions.h>

GraphicRasterizer::ScopedRenderState::ScopedRenderState(
        GraphicRasterizer* rasterizer,
        RenderState* state) :
        rasterizer(rasterizer)
    ,   state(state) {
    rasterizer->saveStateTo(*state);
}

GraphicRasterizer::ScopedRenderState::~ScopedRenderState() {
    rasterizer->restoreStateFrom(*state);
}

GraphicRasterizer::GraphicRasterizer(
            SingletonRepo* repo
        ,   Interactor* interactor
        ,   const String& name
	        ,   int layerGroup
	        ,   bool isCyclic
	        ,   float margin) : SingletonAccessor(repo, name)
	    ,   layerGroup(layerGroup)
	    ,   interactor(interactor) {
    getRequest().cyclic = isCyclic;
    getRequest().xMinimum = -margin;
    getRequest().xMaximum = 1 + margin;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = isCyclic;
    addListener(this);
}

void GraphicRasterizer::pullModPositionAndAdjust() {
    Cycle::Rasterization::GraphicMorphPositionPolicy::Context context;
    context.panelMorph = getObj(MorphPanel).getMorphPosition();
    context.layerGroup = layerGroup;
    context.currentMorphAxis = getSetting(CurrentMorphAxis);

    MeshLibrary::Properties* props = getObj(MeshLibrary).getCurrentProps(layerGroup);

    if (props == nullptr || props->scratchChan == CommonEnums::Null) {
        setMorphPosition(Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context));
        return;
    }

    context.scratchChannel = props->scratchChan;
    context.scratchPosition = getObj(VisualDsp).getScratchPosition(props->scratchChan);
    setMorphPosition(Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context));
}

void GraphicRasterizer::restoreStateFrom(RenderState& state) {
    if (state.restoreMesh) {
        mesh = state.mesh;
    }

    auto& request = getRequest();
    request.lowResCurves = state.lowResCurves;
    request.calcDepthDimensions = state.calcDepthDims;
    request.noiseSeed = state.noiseSeed;
    request.morph = state.pos;
    request.scalingMode = scalingModeFromRenderState(state.scalingType);
    request.batchMode = state.batchMode;
}

void GraphicRasterizer::saveStateTo(RenderState& state) {
    const auto& request = getRequest();
    state.lowResCurves = request.lowResCurves;
    state.calcDepthDims = request.calcDepthDimensions;
    state.noiseSeed = request.noiseSeed;
    state.restoreMesh = true;
    state.mesh = mesh;
    state.pos = request.morph;
    state.scalingType = renderStateScalingType(request.scalingMode);
    state.batchMode = request.batchMode;
}

void GraphicRasterizer::updateGeometry() {
    updateGeometryAtPhase(0.f);
}

void GraphicRasterizer::updateGeometry(Mesh* mesh, float oscPhase) {
    setMesh(mesh);
    updateGeometryAtPhase(oscPhase);
}

void GraphicRasterizer::updateGeometryAtPhase(float oscPhase) {
    prepareRequestForRender();

    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    renderTrilinearGeometry(oscPhase);

    if (!getRequest().batchMode) {
        publishTrilinearSnapshot();
    }
}

void GraphicRasterizer::updateWaveform() {
    updateWaveformAtPhase(0.f);
}

void GraphicRasterizer::updateWaveform(Mesh* mesh, float oscPhase) {
    setMesh(mesh);
    updateWaveformAtPhase(oscPhase);
}

void GraphicRasterizer::updateWaveformAtPhase(float oscPhase) {
    prepareRequestForRender();

    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    renderTrilinearWaveform(oscPhase);

    if (!getRequest().batchMode) {
        publishTrilinearSnapshot();
    }
}

void GraphicRasterizer::cleanUp() {
    if (getRequest().batchMode) {
        clearTrilinearOutput();
        return;
    }

    cleanTrilinearRasterization();
}

Rasterization::PointScalingMode GraphicRasterizer::scalingModeFromRenderState(int scalingType) {
    return Rasterization::pointScalingModeFromLegacy(scalingType);
}

int GraphicRasterizer::renderStateScalingType(Rasterization::PointScalingMode scalingMode) {
    switch (scalingMode) {
        case Rasterization::PointScalingMode::Unipolar:
            return static_cast<int>(Scaling::Unipolar);
        case Rasterization::PointScalingMode::Bipolar:
            return static_cast<int>(Scaling::Bipolar);
        case Rasterization::PointScalingMode::HalfBipolar:
            return static_cast<int>(Scaling::HalfBipolar);
        case Rasterization::PointScalingMode::Identity:
            return static_cast<int>(Scaling::Unipolar);
    }

    return static_cast<int>(Scaling::Unipolar);
}

int GraphicRasterizer::primaryViewDimension() {
    return Cycle::Rasterization::GraphicAxisPolicy().primaryViewDimension(
            repo->get<Settings>("Settings").getGlobalSetting(AppSettings::CurrentMorphAxis));
}

void GraphicRasterizer::prepareRequestForRender() {
    getRequest().primaryViewDimension = primaryViewDimension();
}
