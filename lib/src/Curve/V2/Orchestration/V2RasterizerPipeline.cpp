#include "V2RasterizerPipeline.h"

bool V2RasterizerPipeline::renderArtifacts(V2RasterArtifacts& artifacts) noexcept {
    artifacts.clear();
    if (! renderIntercepts(artifacts)) {
        return false;
    }

    return renderWaveform(artifacts);
}

bool V2RasterizerPipeline::renderBlock(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result
) noexcept {
    result = V2RenderResult{};

    if (output.empty() || ! request.isValid()) {
        return false;
    }

    int numSamples = jmin(request.numSamples, output.size());
    if (numSamples <= 0) {
        return false;
    }

    V2RasterArtifacts artifacts;
    if (! renderArtifacts(artifacts)) {
        return false;
    }

    return sampleArtifacts(artifacts, request, output.withSize(numSamples), result);
}

bool V2RasterizerPipeline::runInterceptStages(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        int& outInterceptCount
) noexcept {
    outInterceptCount = 0;

    if (interpolator_ == nullptr || positioner_ == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    workspace.reset();

    if (! interpolator_->run(interpolatorContext, workspace.intercepts, outInterceptCount)) {
        return false;
    }

    if (outInterceptCount <= 0
            || outInterceptCount > capacities.maxIntercepts
            || static_cast<int>(workspace.intercepts.size()) > capacities.maxIntercepts) {
        return false;
    }

    if (! positioner_->run(workspace.intercepts, outInterceptCount, positionerContext)) {
        return false;
    }

    if (outInterceptCount <= 0
            || outInterceptCount > capacities.maxIntercepts
            || static_cast<int>(workspace.intercepts.size()) > capacities.maxIntercepts) {
        return false;
    }

    return outInterceptCount > 0;
}

bool V2RasterizerPipeline::buildInterceptArtifacts(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        V2RasterArtifacts& outArtifacts
) noexcept {
    outArtifacts = V2RasterArtifacts{};

    int interceptCount = 0;
    if (! runInterceptStages(interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    outArtifacts.intercepts = &workspace.intercepts;
    return interceptCount > 0;
}

bool V2RasterizerPipeline::buildWaveArtifactsFromCurves(
        int curveCount,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts
) noexcept {
    if (waveBuilder_ == nullptr || ! workspace.isPrepared()) {
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

    if (! waveBuilder_->run(
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

bool V2RasterizerPipeline::buildCurveAndWaveArtifacts(
        int interceptCount,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts
) noexcept {
    if (curveBuilder_ == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    const V2CapacitySpec& capacities = workspace.getCapacities();
    if (interceptCount <= 0
            || interceptCount > capacities.maxIntercepts
            || interceptCount > static_cast<int>(workspace.intercepts.size())) {
        return false;
    }

    int curveCount = 0;
    if (! curveBuilder_->run(
            workspace.intercepts,
            interceptCount,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    return buildWaveArtifactsFromCurves(curveCount, waveBuilderContext, outArtifacts);
}

bool V2RasterizerPipeline::buildAllArtifacts(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts
) noexcept {
    outArtifacts = V2RasterArtifacts{};

    if (curveBuilder_ == nullptr || waveBuilder_ == nullptr || ! workspace.isPrepared()) {
        return false;
    }

    if (! buildInterceptArtifacts(interpolatorContext, positionerContext, outArtifacts)) {
        return false;
    }

    return buildCurveAndWaveArtifacts(
        static_cast<int>(workspace.intercepts.size()),
        curveBuilderContext,
        waveBuilderContext,
        outArtifacts);
}
