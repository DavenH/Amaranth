#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2SamplerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerPipeline.h"
#include "V2RasterizerControls.h"
#include "V2RasterizerGraph.h"
#include "V2RasterizerWorkspace.h"

using V2GraphicArtifactsView = V2RasterArtifacts;

class V2GraphicRasterizer :
        public V2RasterizerPipeline {
public:
    V2GraphicRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2GraphicControlSnapshot& snapshot) noexcept;
    bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept override;
    bool renderWaveform(V2RasterArtifacts& artifacts) noexcept override;

    bool renderGraphic(
        const V2GraphicRequest& request,
        Buffer<float> output,
        V2GraphicResult& result) noexcept;

private:
    bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept override;

    V2RasterizerWorkspace workspace;
    V2RasterizerGraph graph;

    V2TrilinearInterpolatorStage interpolator;
    V2ClampOrWrapPositionerStage linearClampPositioner{false};
    V2ClampOrWrapPositionerStage cyclicClampPositioner{true};
    V2ApplyScalingPositionerStage scalingPositioner;
    V2PointPathPositionerStage pointPathPositioner;
    V2SortAndOrderPositionerStage orderPositioner;
    V2CompositePositionerStage linearPositionerPipeline;
    V2CompositePositionerStage cyclicPositionerPipeline;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    const Mesh* mesh{nullptr};
    V2GraphicControlSnapshot controls{};
};
