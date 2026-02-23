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

bool V2FxRasterizer::renderIntercepts(V2RasterArtifacts& artifacts) noexcept {
    artifacts.clear();
    int outCount = 0;

    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext = makePositionerContext(controls);

    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, outCount)) {
        return false;
    }

    artifacts.intercepts = &workspace.intercepts;
    return outCount > 0;
}

bool V2FxRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(
        controls,
        V2CurveBuilderContext::PaddingPolicy::FxLegacyFixed);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);

    int curveCount = 0;
    if (! curveBuilder.run(
            workspace.intercepts,
            static_cast<int>(artifacts.intercepts->size()),
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
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

    artifacts.curves = &workspace.curves;
    artifacts.waveX = workspace.waveX.withSize(wavePointCount);
    artifacts.waveY = workspace.waveY.withSize(wavePointCount);
    artifacts.diffX = workspace.diffX.withSize(jmax(0, wavePointCount - 1));
    artifacts.slope = workspace.slope.withSize(jmax(0, wavePointCount - 1));
    artifacts.zeroIndex = zeroIndex;
    artifacts.oneIndex = oneIndex;
    return wavePointCount > 1;
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

    V2RenderRequest samplerRequest = request;
    samplerRequest.numSamples = numSamples;
    if (samplerRequest.deltaX <= 0.0) {
        samplerRequest.deltaX = 1.0 / static_cast<double>(numSamples);
    }

    V2RasterArtifacts artifacts;
    if (! renderArtifacts(artifacts)) {
        return false;
    }

    V2SamplerContext samplerContext(
        samplerRequest,
        artifacts.waveX.size(),
        artifacts.zeroIndex,
        artifacts.oneIndex);
    result = sampler.run(
        artifacts.waveX,
        artifacts.waveY,
        artifacts.slope,
        output.withSize(numSamples),
        samplerContext);
    return result.rendered;
}
