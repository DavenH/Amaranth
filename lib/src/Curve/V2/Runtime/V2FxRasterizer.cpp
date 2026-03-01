#include "V2FxRasterizer.h"
#include "V2WaveSampling.h"

V2FxRasterizer::V2FxRasterizer() {
    setInterpolator(&interpolator);
    setPositioner(&linearPositionerPipeline);
    setCurveBuilder(&curveBuilder);
    setWaveBuilder(&waveBuilder);

    linearPositionerPipeline.addStage(&linearClampPositioner);
    linearPositionerPipeline.addStage(&scalingPositioner);
    linearPositionerPipeline.addStage(&pointPathPositioner);
    linearPositionerPipeline.addStage(&orderPositioner);
}

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
    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext = makePositionerContext(controls);

    return buildInterceptArtifacts(interpolatorContext, positionerContext, artifacts);
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
    return buildCurveAndWaveArtifacts(
        static_cast<int>(artifacts.intercepts->size()),
        curveBuilderContext,
        waveBuilderContext,
        artifacts);
}

bool V2FxRasterizer::renderAudio(
    const V2RenderRequest& request,
    Buffer<float> output,
    V2RenderResult& result) noexcept {
    return renderBlock(request, output, result);
}

bool V2FxRasterizer::sampleArtifacts(
    const V2RasterArtifacts& artifacts,
    const V2RenderRequest& request,
    Buffer<float> output,
    V2RenderResult& result) noexcept {
    int wavePointCount = artifacts.waveBuffers.waveX.size();
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(output.size());

    LinearPhasePolicy policy(0.0, deltaX);
    int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, output, sampleIndex);
    return result.rendered;
}
