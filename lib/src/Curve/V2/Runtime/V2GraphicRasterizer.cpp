#include "V2GraphicRasterizer.h"

namespace {
int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}
}

V2GraphicRasterizer::V2GraphicRasterizer() :
        graph(&interpolator, &linearPositioner, &curveBuilder, &waveBuilder, &sampler)
{}

void V2GraphicRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
}

void V2GraphicRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
}

void V2GraphicRasterizer::updateControlData(const V2GraphicControlSnapshot& snapshot) noexcept {
    controls = snapshot;
}

bool V2GraphicRasterizer::renderGraphic(
    const V2GraphicRequest& request,
    Buffer<float> output,
    V2GraphicResult& result) noexcept {
    result.rendered = false;
    result.pointsWritten = 0;

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
    curveBuilderContext.interpolateCurves = request.interpolateCurves && controls.interpolateCurves;
    curveBuilderContext.lowResolution = request.lowResolution || controls.lowResolution;
    curveBuilderContext.integralSampling = controls.integralSampling;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = curveBuilderContext.interpolateCurves;

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = numSamples;
    samplerContext.request.deltaX = 1.0 / static_cast<double>(numSamples);
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    V2RenderResult renderResult = graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        output.withSize(numSamples));

    result.rendered = renderResult.rendered;
    result.pointsWritten = renderResult.samplesWritten;
    return result.rendered;
}
