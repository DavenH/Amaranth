#pragma once

#include "../../../Obj/MorphPosition.h"
#include "../../Mesh.h"
#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2SamplerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerGraph.h"
#include "V2RasterizerWorkspace.h"

struct V2FxControlSnapshot {
    MorphPosition morph{};
    MeshRasterizer::ScalingType scaling{MeshRasterizer::Unipolar};
    bool wrapPhases{false};
    bool cyclic{false};
    float minX{0.0f};
    float maxX{1.0f};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};
};

class V2FxRasterizer {
public:
    V2FxRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2FxControlSnapshot& snapshot) noexcept;
    bool extractIntercepts(std::vector<Intercept>& outIntercepts, int& outCount) noexcept;

    bool renderAudio(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept;

private:
    V2RasterizerWorkspace workspace;
    V2RasterizerGraph graph;

    V2FxVertexInterpolatorStage interpolator;
    V2LinearPositionerStage linearPositioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    const Mesh* mesh{nullptr};
    V2FxControlSnapshot controls{};
};
