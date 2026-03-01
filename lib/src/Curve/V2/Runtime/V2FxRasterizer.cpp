#include "V2FxRasterizer.h"

V2FxRasterizer::V2FxRasterizer() :
        graph(&interpolator, &linearPositionerPipeline, &curveBuilder, &waveBuilder, &sampler) {
    // FxRasterizer has no cyclic point positioning
    linearPositionerPipeline.addStage(&linearClampPositioner);
    linearPositionerPipeline.addStage(&scalingPositioner);
    linearPositionerPipeline.addStage(&pointPathPositioner);
    linearPositionerPipeline.addStage(&orderPositioner);
}

void V2FxRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
    graph.setPositioner(&linearPositionerPipeline);
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

    return graph.buildInterceptArtifacts(workspace, interpolatorContext, positionerContext, artifacts);
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
    return graph.buildCurveAndWaveArtifacts(
        workspace,
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
    V2RenderRequest samplerRequest = request;
    samplerRequest.numSamples = output.size();
    if (samplerRequest.deltaX <= 0.0) {
        samplerRequest.deltaX = 1.0 / static_cast<double>(output.size());
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
        output,
        samplerContext);
    return result.rendered;
}
