#include "V2GraphicRasterizer.h"
#include "V2WaveSampling.h"

namespace {
int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}
}

V2GraphicRasterizer::V2GraphicRasterizer() {
    setInterpolator(&interpolator);
    setPositioner(&linearPositionerPipeline);
    setCurveBuilder(&curveBuilder);
    setWaveBuilder(&waveBuilder);

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

    setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositionerPipeline)
                                  : static_cast<V2PositionerStage*>(&linearPositionerPipeline));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(
        mesh,
        controls,
        controls.primaryDimension);
    V2PositionerContext positionerContext = makePositionerContext(controls, controls.primaryDimension);

    return buildInterceptArtifacts(interpolatorContext, positionerContext, artifacts);
}

bool V2GraphicRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    return buildCurveAndWaveArtifacts(
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

    setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositionerPipeline)
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

    V2RasterArtifacts artifacts;
    if (! buildAllArtifacts(
            interpolatorContext,
            positionerContext,
            curveBuilderContext,
            waveBuilderContext,
            artifacts)) {
        return false;
    }

    V2RenderRequest renderRequest{numSamples, 0, 1.0 / static_cast<double>(numSamples), 1.0f, 1, false, false};
    V2RenderResult renderResult;
    if (! sampleArtifacts(artifacts, renderRequest, output.withSize(numSamples), renderResult)) {
        return false;
    }

    result.rendered = renderResult.rendered;
    result.pointsWritten = renderResult.samplesWritten;
    return result.rendered;
}

bool V2GraphicRasterizer::sampleArtifacts(
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
