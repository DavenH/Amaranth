#include "GraphicMeshRasterizer.h"

namespace Rasterization {

GraphicRasterizer::RenderState::RenderState(
        bool lowres,
        bool calcDepth,
        int scaling,
        const MorphPosition& pos) :
        lowResCurves(lowres)
    ,   calcDepthDims(calcDepth)
    ,   scalingType(scaling)
    ,   pos(pos) {
}

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

GraphicRasterizer::GraphicRasterizer(bool cyclic, float margin) {
    auto& request = compatibilityRequest();
    request.cyclic = cyclic;
    request.xMinimum = -margin;
    request.xMaximum = 1.f + margin;
    rasterizerData.paddingSize = getPaddingSize();
    rasterizerData.wrapsVertices = cyclic;
}

void GraphicRasterizer::restoreStateFrom(RenderState& state) {
    if (state.restoreMesh) {
        mesh = state.mesh;
    }

    auto& request = compatibilityRequest();
    request.lowResCurves = state.lowResCurves;
    request.calcDepthDimensions = state.calcDepthDims;
    request.noiseSeed = state.noiseSeed;
    request.morph = state.pos;
    request.scalingMode = scalingModeFromRenderState(state.scalingType);
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
}

void GraphicRasterizer::updateGeometry() {
    updateGeometryAtPhase(0.f);
}

void GraphicRasterizer::updateGeometry(Mesh* mesh, float oscPhase) {
    setMesh(mesh);
    updateGeometryAtPhase(oscPhase);
}

void GraphicRasterizer::updateGeometryAtPhase(float oscPhase) {
    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    renderTrilinearGeometry(oscPhase);

    publishTrilinearSnapshot();
}

void GraphicRasterizer::updateWaveform() {
    updateWaveformAtPhase(0.f);
}

void GraphicRasterizer::updateWaveform(Mesh* mesh, float oscPhase) {
    setMesh(mesh);
    updateWaveformAtPhase(oscPhase);
}

void GraphicRasterizer::updateWaveformAtPhase(float oscPhase) {
    if (mesh == nullptr || mesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    renderTrilinearWaveform(oscPhase);

    publishTrilinearSnapshot();
}

void GraphicRasterizer::cleanUp() {
    cleanTrilinearRasterization();
}

GraphicRasterizer::RenderState GraphicRasterizer::createRenderState() {
    RenderState state;
    saveStateTo(state);
    return state;
}

GraphicRasterizer::RenderState GraphicRasterizer::createAnalysisRenderState(
        Scaling scaling,
        const MorphPosition& morphPosition,
        bool lowResCurves,
        bool calcDepthDimensions) {
    return RenderState(
            lowResCurves,
            calcDepthDimensions,
            static_cast<int>(scaling),
            morphPosition);
}

PointScalingMode GraphicRasterizer::scalingModeFromRenderState(int scalingType) {
    return pointScalingModeFromLegacy(scalingType);
}

int GraphicRasterizer::renderStateScalingType(PointScalingMode scalingMode) {
    switch (scalingMode) {
        case PointScalingMode::Unipolar:
            return static_cast<int>(Scaling::Unipolar);
        case PointScalingMode::Bipolar:
            return static_cast<int>(Scaling::Bipolar);
        case PointScalingMode::HalfBipolar:
            return static_cast<int>(Scaling::HalfBipolar);
        case PointScalingMode::Identity:
            return static_cast<int>(Scaling::Unipolar);
    }

    return static_cast<int>(Scaling::Unipolar);
}

}
