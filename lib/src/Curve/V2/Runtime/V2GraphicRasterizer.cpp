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

bool V2GraphicRasterizer::extractIntercepts(std::vector<Intercept>& outIntercepts, int& outCount) noexcept {
    outIntercepts.clear();
    outCount = 0;

    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls);

    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, outCount)) {
        return false;
    }

    outIntercepts.assign(workspace.intercepts.begin(), workspace.intercepts.begin() + outCount);
    return outCount > 0;
}

bool V2GraphicRasterizer::extractArtifacts(V2GraphicArtifactsView& outArtifacts) noexcept {
    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls);
    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    return graph.buildArtifacts(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        outArtifacts);
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

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls);
    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(
        controls,
        request.interpolateCurves,
        request.lowResolution);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(
        controls,
        curveBuilderContext.interpolateCurves);

    V2SamplerContext samplerContext(
        V2RenderRequest{numSamples, 0, 1.0 / static_cast<double>(numSamples), 1.0f, 1, false, false});

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
