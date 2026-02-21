#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2SamplerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "V2RasterizerControls.h"
#include "V2RasterizerGraph.h"
#include "V2RasterizerWorkspace.h"

class V2VoiceRasterizer {
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

private:
    bool buildWave(int& wavePointCount, int& zeroIndex, int& oneIndex) noexcept;

    V2RasterizerWorkspace workspace;
    V2RasterizerGraph graph;

    V2TrilinearInterpolatorStage interpolator;
    V2LinearPositionerStage linearPositioner;
    V2VoiceChainingPositionerStage chainingPositioner;
    V2VoiceChainingCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    const Mesh* mesh{nullptr};
    V2VoiceControlSnapshot controls{};
    double phase{0.0};
    int sampleIndex{0};
    float cycleStartX{0.0f};
    float cycleEndX{1.0f};
};
