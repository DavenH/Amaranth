#pragma once

#include <vector>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Intercept.h>
#include <Obj/ColorPoint.h>
#include <Curve/V2/Stages/V2StageInterfaces.h>
#include <Curve/V2/Core/V2RenderTypes.h>
#include "../Core/V2RasterizerWorkspace.h"

struct V2RasterArtifacts {
    const std::vector<Intercept>* intercepts{nullptr};

    const std::vector<Curve>* curves{nullptr};

    const std::vector<ColorPoint>* colorPoints{nullptr};
    const std::vector<V2DeformRegion>* deformRegions{nullptr};

    V2WaveBuffers waveBuffers;
    int zeroIndex{0};
    int oneIndex{0};

    void clear() noexcept {
        *this = V2RasterArtifacts{};
    }
};

class V2RasterizerPipeline {
public:
    virtual ~V2RasterizerPipeline() = default;

    bool renderArtifacts(V2RasterArtifacts& artifacts) noexcept;

    bool renderBlock(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept;

    virtual bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept = 0;
    virtual bool renderWaveform(V2RasterArtifacts& artifacts) noexcept = 0;

protected:
    virtual bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept = 0;

    void setInterpolator(V2InterpolatorStage* stage) noexcept { interpolator_ = stage; }
    void setPositioner(V2PositionerStage* stage) noexcept { positioner_ = stage; }
    void setCurveBuilder(V2CurveBuilderStage* stage) noexcept { curveBuilder_ = stage; }
    void setWaveBuilder(V2WaveBuilderStage* stage) noexcept { waveBuilder_ = stage; }

    bool runInterceptStages(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        int& outInterceptCount) noexcept;

    bool buildInterceptArtifacts(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        V2RasterArtifacts& outArtifacts) noexcept;

    bool buildWaveArtifactsFromCurves(
        int curveCount,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts) noexcept;

    bool buildCurveAndWaveArtifacts(
        int interceptCount,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts) noexcept;

    bool buildAllArtifacts(
        const V2InterpolatorContext& interpolatorContext,
        const V2PositionerContext& positionerContext,
        const V2CurveBuilderContext& curveBuilderContext,
        const V2WaveBuilderContext& waveBuilderContext,
        V2RasterArtifacts& outArtifacts) noexcept;

    V2RasterizerWorkspace workspace;

private:
    V2InterpolatorStage* interpolator_{nullptr};
    V2PositionerStage* positioner_{nullptr};
    V2CurveBuilderStage* curveBuilder_{nullptr};
    V2WaveBuilderStage* waveBuilder_{nullptr};
};
