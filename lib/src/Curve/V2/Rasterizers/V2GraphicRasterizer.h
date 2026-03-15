#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "../Orchestration/V2RasterizerPipeline.h"
#include "../Orchestration/V2RasterizerControls.h"

class V2GraphicRasterizer :
        public V2RasterizerPipeline {
public:
    V2GraphicRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2GraphicControlSnapshot& snapshot) noexcept;
    bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept override;
    bool renderWaveform(V2RasterArtifacts& artifacts) noexcept override;

private:
    bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept override;

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

    const Mesh* mesh{nullptr};
    V2GraphicControlSnapshot controls{};
};
