#include "V2VoiceRasterizer.h"
#include "../Sampling/V2WaveSampling.h"

V2VoiceRasterizer::V2VoiceRasterizer() {
    setInterpolator(&interpolator);
    setPositioner(&chainingPositionerPipeline);
    setCurveBuilder(&curveBuilder);
    setWaveBuilder(&waveBuilder);

    chainingPositionerPipeline.addStage(&cyclicClampPositioner);
    chainingPositionerPipeline.addStage(&scalingPositioner);
    chainingPositionerPipeline.addStage(&pointPathPositioner);
    chainingPositionerPipeline.addStage(&orderPositioner);
    chainingPositionerPipeline.addStage(&chainingPositioner);
}

void V2VoiceRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
    chainingPositionerPipeline.reset();
    curveBuilder.reset();
    previousBackIntercepts.clear();
    currentBackIntercepts.clear();
    chainingCallCount = 0;
    chainingAdvancement = 0.0f;
}

void V2VoiceRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
    chainingPositionerPipeline.reset();
    curveBuilder.reset();
    previousBackIntercepts.clear();
    currentBackIntercepts.clear();
    chainingCallCount = 0;
    chainingAdvancement = 0.0f;
}

void V2VoiceRasterizer::updateControlData(const V2VoiceControlSnapshot& snapshot) noexcept {
    controls = snapshot;
    controls.cyclic = true;
}

void V2VoiceRasterizer::resetPhase(double phase) noexcept {
    this->phase = phase;
    sampleIndex = 0;
    chainingPositionerPipeline.reset();
    curveBuilder.reset();
    previousBackIntercepts.clear();
    currentBackIntercepts.clear();
    chainingCallCount = 0;
    chainingAdvancement = 0.0f;
}

double V2VoiceRasterizer::getPhaseForTesting() const noexcept {
    return phase;
}

bool V2VoiceRasterizer::sampleArtifacts(
    const V2RasterArtifacts& artifacts,
    const V2RenderRequest& request,
    Buffer<float> out,
    V2RenderResult& result) noexcept {
    int wavePointCount = artifacts.waveBuffers.waveX.size();
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(out.size());

    VoiceFoldPhasePolicy policy(phase, deltaX);
    result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, out, sampleIndex);
    phase = policy.phase;
    return result.rendered;
}

bool V2VoiceRasterizer::renderIntercepts(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    if (chainingCallCount > 0 && chainingAdvancement > 0.0f) {
        interpolatorContext.morph.time.setValueDirect(jmin(
            1.0f,
            interpolatorContext.morph.time.getCurrentValue() + chainingAdvancement));
    }
    V2PositionerContext positionerContext = makePositionerContext(controls);
    positionerContext.cyclic = true;

    if (! buildInterceptArtifacts(interpolatorContext, positionerContext, artifacts)) {
        return false;
    }

    previousBackIntercepts = currentBackIntercepts;
    currentBackIntercepts.assign(
        artifacts.intercepts->begin(),
        artifacts.intercepts->end());

    if (chainingCallCount == 0 && controls.minLineLength > 0.0f) {
        chainingAdvancement = controls.minLineLength * 1.1f;
    }

    ++chainingCallCount;
    return true;
}

bool V2VoiceRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    if (! previousBackIntercepts.empty()) {
        curveBuilder.prime(previousBackIntercepts);
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    return buildCurveAndWaveArtifacts(
        static_cast<int>(artifacts.intercepts->size()),
        curveBuilderContext,
        waveBuilderContext,
        artifacts);
}
