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

bool V2RasterizerGraph::buildInterceptArtifacts(
    V2RasterizerWorkspace& workspace,
    const V2InterpolatorContext& interpolatorContext,
    const V2PositionerContext& positionerContext,
    V2BuiltArtifacts& outArtifacts) noexcept {
    outArtifacts = V2BuiltArtifacts{};

    int interceptCount = 0;
    if (! runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    outArtifacts.intercepts = &workspace.intercepts;
    return interceptCount > 0;
}

bool V2RasterizerGraph::buildWaveArtifactsFromCurves(
        V2RasterizerWorkspace& workspace,
        int curveCount,
        const V2WaveBuilderContext& waveBuilderContext,
        V2BuiltArtifacts& outArtifacts) noexcept {
    if (waveBuilder == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    if (curveCount <= 0
            || curveCount > capacities.maxCurves
            || curveCount > static_cast<int>(workspace.curves.size())
            || static_cast<int>(workspace.curves.size()) > capacities.maxCurves) {
        return false;
    }

    int zeroIndex = 0;
    int oneIndex = 0;
    int wavePointCount = 0;
    V2WaveBuilderContext localWaveBuilderContext = waveBuilderContext;
    localWaveBuilderContext.componentPath.deformRegions = &workspace.deformRegions;

    if (! waveBuilder->run(
            workspace.curves,
            curveCount,
            workspace.waveBuffers,
            wavePointCount,
            zeroIndex,
            oneIndex,
            localWaveBuilderContext)) {
        return false;
    }

    if (wavePointCount <= 1
            || wavePointCount > capacities.maxWavePoints
            || ! workspace.waveBuffers.canContain(wavePointCount)) {
        return false;
    }

    outArtifacts.curves = &workspace.curves;
    workspace.waveBuffers.assignSized(outArtifacts.waveBuffers, wavePointCount);
    outArtifacts.deformRegions = &workspace.deformRegions;
    outArtifacts.zeroIndex = zeroIndex;
    outArtifacts.oneIndex = oneIndex;
    return true;
}

bool V2RasterizerGraph::buildCurveAndWaveArtifacts(
    V2RasterizerWorkspace& workspace,
    int interceptCount,
    const V2CurveBuilderContext& curveBuilderContext,
    const V2WaveBuilderContext& waveBuilderContext,
    V2BuiltArtifacts& outArtifacts) noexcept {
    if (curveBuilder == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    if (interceptCount <= 0
            || interceptCount > capacities.maxIntercepts
            || interceptCount > static_cast<int>(workspace.intercepts.size())) {
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

    return buildWaveArtifactsFromCurves(workspace, curveCount, waveBuilderContext, outArtifacts);
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

    if (! buildInterceptArtifacts(workspace, interpolatorContext, positionerContext, outArtifacts)) {
        return false;
    }

    return buildCurveAndWaveArtifacts(
        workspace,
        static_cast<int>(workspace.intercepts.size()),
        curveBuilderContext,
        waveBuilderContext,
        outArtifacts);
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
        result.stillActive = artifacts.intercepts != nullptr && ! artifacts.intercepts->empty();
        result.samplesWritten = 0;
        return result;
    }

    V2SamplerContext context = samplerContext;
    context.setWaveState(artifacts.waveBuffers.waveX.size(), artifacts.zeroIndex, artifacts.oneIndex);
    context.setDecoupledRegions(artifacts.deformRegions);
    context.configureDecoupledDeform(waveBuilderContext.componentPath);

    return sampler->run(artifacts.waveBuffers, output, context);
}
