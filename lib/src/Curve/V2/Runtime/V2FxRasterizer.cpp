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

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.scaling = controls.scaling;
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

    result = graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        output.withSize(numSamples));
    return result.rendered;
}
