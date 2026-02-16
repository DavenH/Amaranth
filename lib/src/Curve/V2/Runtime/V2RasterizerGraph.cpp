#include "V2RasterizerGraph.h"

V2RasterizerGraph::V2RasterizerGraph(
    V2InterpolatorStage* interpolator,
    V2PositionerStage* positioner,
    V2CurveBuilderStage* curveBuilder,
    V2WaveBuilderStage* waveBuilder,
    V2SamplerStage* sampler) :
        interpolator(interpolator)
    ,   positioner(positioner)
    ,   curveBuilder(curveBuilder)
    ,   waveBuilder(waveBuilder)
    ,   sampler(sampler)
{}

void V2RasterizerGraph::setInterpolator(V2InterpolatorStage* stage) noexcept {
    interpolator = stage;
}

void V2RasterizerGraph::setPositioner(V2PositionerStage* stage) noexcept {
    positioner = stage;
}

void V2RasterizerGraph::setCurveBuilder(V2CurveBuilderStage* stage) noexcept {
    curveBuilder = stage;
}

void V2RasterizerGraph::setWaveBuilder(V2WaveBuilderStage* stage) noexcept {
    waveBuilder = stage;
}

void V2RasterizerGraph::setSampler(V2SamplerStage* stage) noexcept {
    sampler = stage;
}

bool V2RasterizerGraph::runInterceptStages(
    V2RasterizerWorkspace& workspace,
    const V2InterpolatorContext& interpolatorContext,
    const V2PositionerContext& positionerContext,
    int& outInterceptCount) noexcept {
    outInterceptCount = 0;

    if (interpolator == nullptr || positioner == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    workspace.reset();

    if (! interpolator->run(interpolatorContext, workspace.intercepts, outInterceptCount)) {
        return false;
    }

    if (! positioner->run(workspace.intercepts, outInterceptCount, positionerContext)) {
        return false;
    }

    return outInterceptCount > 0;
}

V2RenderResult V2RasterizerGraph::render(
    V2RasterizerWorkspace& workspace,
    const V2InterpolatorContext& interpolatorContext,
    const V2PositionerContext& positionerContext,
    const V2CurveBuilderContext& curveBuilderContext,
    const V2WaveBuilderContext& waveBuilderContext,
    const V2SamplerContext& samplerContext,
    Buffer<float> output) noexcept {
    V2RenderResult result;

    int interceptCount = 0;
    if (! runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return result;
    }

    if (curveBuilder == nullptr || waveBuilder == nullptr || sampler == nullptr) {
        result.rendered = true;
        result.stillActive = interceptCount > 0;
        result.samplesWritten = 0;
        return result;
    }

    int curveCount = 0;
    if (! curveBuilder->run(
            workspace.intercepts,
            interceptCount,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return result;
    }

    int zeroIndex = 0;
    int oneIndex = 0;

    if (! waveBuilder->run(
            workspace.curves,
            curveCount,
            workspace.waveX,
            workspace.waveY,
            workspace.diffX,
            workspace.slope,
            zeroIndex,
            oneIndex,
            waveBuilderContext)) {
        return result;
    }

    V2SamplerContext context = samplerContext;
    context.zeroIndex = zeroIndex;
    context.oneIndex = oneIndex;

    return sampler->run(
        workspace.waveX,
        workspace.waveY,
        workspace.slope,
        output,
        context);
}

