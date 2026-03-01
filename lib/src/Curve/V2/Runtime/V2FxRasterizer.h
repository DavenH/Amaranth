#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerPipeline.h"
#include "V2RasterizerControls.h"

class V2FxRasterizer :
        public V2RasterizerPipeline {
public:
    V2FxRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2FxControlSnapshot& snapshot) noexcept;
    bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept override;
    bool renderWaveform(V2RasterArtifacts& artifacts) noexcept override;

    bool renderAudio(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept;

private:
    bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept override;

    V2FxVertexInterpolatorStage interpolator;
    V2ClampOrWrapPositionerStage linearClampPositioner{false};
    V2ClampOrWrapPositionerStage cyclicClampPositioner{true};
    V2ApplyScalingPositionerStage scalingPositioner;
    V2PointPathPositionerStage pointPathPositioner;
    V2SortAndOrderPositionerStage orderPositioner;
    V2CompositePositionerStage linearPositionerPipeline;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;

    const Mesh* mesh{nullptr};
    V2FxControlSnapshot controls{};
};
