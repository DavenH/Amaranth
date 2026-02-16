#pragma once

#include "../Stages/V2StageInterfaces.h"
#include "V2RasterizerWorkspace.h"

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

