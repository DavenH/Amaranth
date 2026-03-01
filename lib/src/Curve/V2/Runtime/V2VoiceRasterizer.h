#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerPipeline.h"
#include "V2RasterizerControls.h"

class V2VoiceRasterizer :
        public V2RasterizerPipeline {
public:
    V2VoiceRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2VoiceControlSnapshot& snapshot) noexcept;
    void resetPhase(double phase = 0.0) noexcept;
    double getPhaseForTesting() const noexcept;

    bool renderAudio(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept;
    bool renderIntercepts(V2RasterArtifacts& artifacts) noexcept override;
    bool renderWaveform(V2RasterArtifacts& artifacts) noexcept override;

private:
    bool sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept override;

    V2TrilinearInterpolatorStage interpolator;
    V2ClampOrWrapPositionerStage cyclicClampPositioner{true};
    V2ApplyScalingPositionerStage scalingPositioner;
    V2PointPathPositionerStage pointPathPositioner;
    V2SortAndOrderPositionerStage orderPositioner;
    V2VoiceChainingPositionerStage chainingPositioner;
    V2CompositePositionerStage chainingPositionerPipeline;
    V2VoiceChainingCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;

    const Mesh* mesh{nullptr};
    V2VoiceControlSnapshot controls{};
    int chainingCallCount{0};
    float chainingAdvancement{0.0f};
    double phase{0.0};
    int sampleIndex{0};
};
