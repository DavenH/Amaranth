#include "V2FxRasterizer.h"

namespace {
int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}
}

V2FxRasterizer::V2FxRasterizer() :
        graph(&interpolator, &linearPositioner, &curveBuilder, &waveBuilder, &sampler)
{}

void V2FxRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
}

void V2FxRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
}

void V2FxRasterizer::updateControlData(const V2FxControlSnapshot& snapshot) noexcept {
    controls = snapshot;
}

bool V2FxRasterizer::extractIntercepts(std::vector<Intercept>& outIntercepts, int& outCount) noexcept {
    outIntercepts.clear();
    outCount = 0;

    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext;
    interpolatorContext.mesh = mesh;
    interpolatorContext.morph = controls.morph;
    interpolatorContext.wrapPhases = controls.wrapPhases;

    V2PositionerContext positionerContext;
    positionerContext.scaling = controls.scaling;
    positionerContext.cyclic = controls.cyclic;
    positionerContext.minX = controls.minX;
    positionerContext.maxX = controls.maxX;

    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, outCount)) {
        return false;
    }

    outIntercepts.assign(workspace.intercepts.begin(), workspace.intercepts.begin() + outCount);
    return outCount > 0;
}

bool V2FxRasterizer::renderAudio(
    const V2RenderRequest& request,
    Buffer<float> output,
    V2RenderResult& result) noexcept {
    result = V2RenderResult{};

    if (! workspace.isPrepared() || mesh == nullptr || output.empty() || ! request.isValid()) {
        return false;
    }

    int numSamples = clampRequestedSamples(request.numSamples, output);
    if (numSamples <= 0) {
        return false;
    }

    std::vector<Intercept> intercepts;
    int interceptCount = 0;
    if (! extractIntercepts(intercepts, interceptCount) || interceptCount <= 1) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.scaling = controls.scaling;
    curveBuilderContext.paddingPolicy = V2CurveBuilderContext::PaddingPolicy::FxLegacyFixed;
    curveBuilderContext.interpolateCurves = controls.interpolateCurves;
    curveBuilderContext.lowResolution = controls.lowResolution;
    curveBuilderContext.integralSampling = controls.integralSampling;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = controls.interpolateCurves;

    V2SamplerContext samplerContext;
    samplerContext.request = request;
    samplerContext.request.numSamples = numSamples;
    if (samplerContext.request.deltaX <= 0.0) {
        samplerContext.request.deltaX = 1.0 / static_cast<double>(numSamples);
    }

    int curveCount = 0;
    workspace.curves.clear();
    if (! curveBuilder.run(intercepts, interceptCount, workspace.curves, curveCount, curveBuilderContext)) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    if (curveCount <= 0
            || curveCount > capacities.maxCurves
            || static_cast<int>(workspace.curves.size()) > capacities.maxCurves) {
        return false;
    }

    int zeroIndex = 0;
    int oneIndex = 0;
    int wavePointCount = 0;
    if (! waveBuilder.run(
            workspace.curves,
            curveCount,
            workspace.waveX,
            workspace.waveY,
            workspace.diffX,
            workspace.slope,
            wavePointCount,
            zeroIndex,
            oneIndex,
            waveBuilderContext)) {
        return false;
    }

    samplerContext.wavePointCount = wavePointCount;
    samplerContext.zeroIndex = zeroIndex;
    samplerContext.oneIndex = oneIndex;
    result = sampler.run(
        workspace.waveX.withSize(wavePointCount),
        workspace.waveY.withSize(wavePointCount),
        workspace.slope.withSize(wavePointCount - 1),
        output.withSize(numSamples),
        samplerContext);
    return result.rendered;
}
