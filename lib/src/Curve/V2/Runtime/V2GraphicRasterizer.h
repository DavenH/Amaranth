#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2SamplerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerControls.h"
#include "V2RasterizerGraph.h"
#include "V2RasterizerWorkspace.h"

using V2GraphicArtifactsView = V2BuiltArtifacts;

class V2GraphicRasterizer {
public:
    V2GraphicRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2GraphicControlSnapshot& snapshot) noexcept;
    bool extractIntercepts(std::vector<Intercept>& outIntercepts, int& outCount) noexcept;
    bool extractArtifacts(V2GraphicArtifactsView& outArtifacts) noexcept;

    bool renderGraphic(
        const V2GraphicRequest& request,
        Buffer<float> output,
        V2GraphicResult& result) noexcept;

private:
    V2RasterizerWorkspace workspace;
    V2RasterizerGraph graph;

    V2TrilinearInterpolatorStage interpolator;
    V2LinearPositionerStage linearPositioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    const Mesh* mesh{nullptr};
    V2GraphicControlSnapshot controls{};
};
