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

    const V2CapacitySpec& capacities = workspace.getCapacities();
    workspace.reset();

    if (! interpolator->run(interpolatorContext, workspace.intercepts, outInterceptCount)) {
        return false;
    }

    if (outInterceptCount <= 0
            || outInterceptCount > capacities.maxIntercepts
            || static_cast<int>(workspace.intercepts.size()) > capacities.maxIntercepts) {
        return false;
    }

    if (! positioner->run(workspace.intercepts, outInterceptCount, positionerContext)) {
        return false;
    }

    if (outInterceptCount <= 0
            || outInterceptCount > capacities.maxIntercepts
            || static_cast<int>(workspace.intercepts.size()) > capacities.maxIntercepts) {
        return false;
    }

    return outInterceptCount > 0;
}

bool V2RasterizerGraph::buildArtifacts(
    V2RasterizerWorkspace& workspace,
    const V2InterpolatorContext& interpolatorContext,
    const V2PositionerContext& positionerContext,
    const V2CurveBuilderContext& curveBuilderContext,
    const V2WaveBuilderContext& waveBuilderContext,
    V2BuiltArtifacts& outArtifacts) noexcept {
    outArtifacts = V2BuiltArtifacts{};

    if (curveBuilder == nullptr || waveBuilder == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    int interceptCount = 0;
    if (! runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    int curveCount = 0;
    if (! curveBuilder->run(
            workspace.intercepts,
            interceptCount,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    if (curveCount <= 0
            || curveCount > capacities.maxCurves
            || static_cast<int>(workspace.curves.size()) > capacities.maxCurves) {
        return false;
    }

    int zeroIndex = 0;
    int oneIndex = 0;
    int wavePointCount = 0;
    if (! waveBuilder->run(
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

    outArtifacts.intercepts = &workspace.intercepts;
    outArtifacts.interceptCount = interceptCount;
    outArtifacts.curves = &workspace.curves;
    outArtifacts.curveCount = curveCount;
    outArtifacts.waveX = workspace.waveX.withSize(wavePointCount);
    outArtifacts.waveY = workspace.waveY.withSize(wavePointCount);
    outArtifacts.diffX = workspace.diffX.withSize(jmax(0, wavePointCount - 1));
    outArtifacts.slope = workspace.slope.withSize(jmax(0, wavePointCount - 1));
    outArtifacts.wavePointCount = wavePointCount;
    outArtifacts.zeroIndex = zeroIndex;
    outArtifacts.oneIndex = oneIndex;
    return wavePointCount > 1;
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

    if (curveBuilder == nullptr || waveBuilder == nullptr) {
        int interceptCount = 0;
        if (! runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
            return result;
        }

        result.rendered = true;
        result.stillActive = interceptCount > 0;
        result.samplesWritten = 0;
        return result;
    }

    V2BuiltArtifacts artifacts;
    if (! buildArtifacts(
            workspace,
            interpolatorContext,
            positionerContext,
            curveBuilderContext,
            waveBuilderContext,
            artifacts)) {
        return result;
    }

    if (sampler == nullptr) {
        result.rendered = true;
        result.stillActive = artifacts.interceptCount > 0;
        result.samplesWritten = 0;
        return result;
    }

    V2SamplerContext context = samplerContext;
    context.wavePointCount = artifacts.wavePointCount;
    context.zeroIndex = artifacts.zeroIndex;
    context.oneIndex = artifacts.oneIndex;

    return sampler->run(
        artifacts.waveX,
        artifacts.waveY,
        artifacts.slope,
        output,
        context);
}
