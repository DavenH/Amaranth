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
        graph(&interpolator, &linearPositionerPipeline, &curveBuilder, &waveBuilder, &sampler) {
    linearPositionerPipeline.addStage(&linearClampPositioner);
    linearPositionerPipeline.addStage(&scalingPositioner);
    linearPositionerPipeline.addStage(&pointPathPositioner);
    linearPositionerPipeline.addStage(&orderPositioner);

    cyclicPositionerPipeline.addStage(&cyclicClampPositioner);
    cyclicPositionerPipeline.addStage(&scalingPositioner);
    cyclicPositionerPipeline.addStage(&pointPathPositioner);
    cyclicPositionerPipeline.addStage(&orderPositioner);
}

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

bool V2GraphicRasterizer::renderIntercepts(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositionerPipeline)
                                        : static_cast<V2PositionerStage*>(&linearPositionerPipeline));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls, controls.primaryDimension);

    return graph.buildInterceptArtifacts(workspace, interpolatorContext, positionerContext, artifacts);
}

bool V2GraphicRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    return graph.buildCurveAndWaveArtifacts(
        workspace,
        static_cast<int>(artifacts.intercepts->size()),
        curveBuilderContext,
        waveBuilderContext,
        artifacts);
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

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositionerPipeline)
                                        : static_cast<V2PositionerStage*>(&linearPositionerPipeline));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls, controls.primaryDimension);
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

bool V2GraphicRasterizer::sampleArtifacts(
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
        artifacts.waveBuffers.waveX.size(),
        artifacts.zeroIndex,
        artifacts.oneIndex);
    result = sampler.run(artifacts.waveBuffers, output, samplerContext);
    return result.rendered;
}
