#pragma once

#include "../Stages/V2CurveBuilderStages.h"
#include "../Stages/V2InterpolatorStages.h"
#include "../Stages/V2PositionerStages.h"
#include "../Stages/V2SamplerStages.h"
#include "../Stages/V2WaveBuilderStages.h"
#include "../State/V2EnvStateMachine.h"
#include "V2RasterizerControls.h"
#include "V2RasterizerGraph.h"
#include "V2RasterizerWorkspace.h"

class V2EnvRasterizer {
public:
    V2EnvRasterizer();

    void prepare(const V2PrepareSpec& spec);
    void setMeshSnapshot(const Mesh* meshSnapshot) noexcept;
    void updateControlData(const V2EnvControlSnapshot& snapshot) noexcept;

    void noteOn() noexcept;
    bool noteOff() noexcept;
    bool transitionToLooping(bool canLoop, bool overextends) noexcept;
    bool consumeReleaseTrigger() noexcept;
    V2EnvMode getMode() const noexcept;
    bool isReleasePending() const noexcept;
    double getSamplePositionForTesting() const noexcept;

    bool renderAudio(
        const V2RenderRequest& request,
        Buffer<float> output,
        V2RenderResult& result) noexcept;

    void setInterpolatorForTesting(V2InterpolatorStage* stage) noexcept;

private:
    bool buildWaveForCurrentState(
        const V2RenderRequest& request,
        int numSamples,
        int& wavePointCount,
        int& zeroIndex,
        int& oneIndex) noexcept;

    V2RasterizerWorkspace workspace;
    V2RasterizerGraph graph;

    V2TrilinearInterpolatorStage interpolator;
    V2LinearPositionerStage linearPositioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    V2EnvStateMachine envState;
    V2InterpolatorStage* testInterpolator{nullptr};
    const Mesh* mesh{nullptr};
    V2EnvControlSnapshot controls{};
    double samplePosition{0.0};
    int sampleIndex{0};
};
