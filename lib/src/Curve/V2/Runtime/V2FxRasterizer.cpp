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

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext = makePositionerContext(controls);

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

    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext = makePositionerContext(controls);
    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(
        controls,
        V2CurveBuilderContext::PaddingPolicy::FxLegacyFixed);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);

    V2RenderRequest samplerRequest = request;
    samplerRequest.numSamples = numSamples;
    if (samplerRequest.deltaX <= 0.0) {
        samplerRequest.deltaX = 1.0 / static_cast<double>(numSamples);
    }

    V2BuiltArtifacts artifacts;
    if (! graph.buildArtifacts(
            workspace,
            interpolatorContext,
            positionerContext,
            curveBuilderContext,
            waveBuilderContext,
            artifacts)) {
        return false;
    }

    V2SamplerContext samplerContext(
        samplerRequest,
        artifacts.wavePointCount,
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
