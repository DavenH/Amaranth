#pragma once

#include "../Stages/V2StageInterfaces.h"
#include "V2RasterizerPipeline.h"
#include "V2RasterizerWorkspace.h"

using V2BuiltArtifacts = V2RasterArtifacts;

class V2RasterizerGraph {
public:
    V2RasterizerGraph() = default;
    V2RasterizerGraph(
        V2InterpolatorStage* interpolator,
        V2PositionerStage* positioner,
        V2CurveBuilderStage* curveBuilder = nullptr,
        V2WaveBuilderStage* waveBuilder = nullptr,
        V2SamplerStage* sampler = nullptr);

    void setInterpolator(V2InterpolatorStage* stage) noexcept;
    void setPositioner(V2PositionerStage* stage) noexcept;
    void setCurveBuilder(V2CurveBuilderStage* stage) noexcept;
    void setWaveBuilder(V2WaveBuilderStage* stage) noexcept;
    void setSampler(V2SamplerStage* stage) noexcept;

    bool runInterceptStages(
        V2RasterizerWorkspace& workspace,
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        int& outInterceptCount) noexcept;

    bool buildInterceptArtifacts(
        V2RasterizerWorkspace& workspace,
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        V2BuiltArtifacts& outArtifacts) noexcept;

    bool buildWaveArtifactsFromCurves(
        V2RasterizerWorkspace& workspace,
        int curveCount,
        const V2WaveBuilderContext& waveBuilderContext,
        V2BuiltArtifacts& outArtifacts) noexcept;

    bool buildCurveAndWaveArtifacts(
        V2RasterizerWorkspace& workspace,
        int interceptCount,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2BuiltArtifacts& outArtifacts) noexcept;

    bool buildArtifacts(
        V2RasterizerWorkspace& workspace,
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2BuiltArtifacts& outArtifacts) noexcept;

    V2RenderResult render(
        V2RasterizerWorkspace& workspace,
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        const V2SamplerContext& samplerContext,
        Buffer<float> output) noexcept;

private:
    V2InterpolatorStage* interpolator{nullptr};
    V2PositionerStage* positioner{nullptr};
    V2CurveBuilderStage* curveBuilder{nullptr};
    V2WaveBuilderStage* waveBuilder{nullptr};
    V2SamplerStage* sampler{nullptr};
};
