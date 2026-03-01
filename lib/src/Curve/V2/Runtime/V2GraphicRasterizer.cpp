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
    artifacts.clear();
    int outCount = 0;

    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositionerPipeline)
                                        : static_cast<V2PositionerStage*>(&linearPositionerPipeline));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls);

    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, outCount)) {
        return false;
    }

    artifacts.intercepts = &workspace.intercepts;
    return outCount > 0;
}

bool V2GraphicRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
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
