#include "V2VoiceRasterizer.h"
#include "V2WaveSampling.h"

namespace {
}

V2VoiceRasterizer::V2VoiceRasterizer() :
        graph(&interpolator, &chainingPositionerPipeline, &curveBuilder, &waveBuilder, &sampler) {
    chainingPositionerPipeline.addStage(&cyclicClampPositioner);
    chainingPositionerPipeline.addStage(&scalingPositioner);
    chainingPositionerPipeline.addStage(&pointPathPositioner);
    chainingPositionerPipeline.addStage(&orderPositioner);
    chainingPositionerPipeline.addStage(&chainingPositioner);
    graph.setPositioner(&chainingPositionerPipeline);
}

void V2VoiceRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
    chainingPositionerPipeline.reset();
    curveBuilder.reset();
    chainingCallCount = 0;
    chainingAdvancement = 0.0f;
}

void V2VoiceRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
    chainingPositionerPipeline.reset();
    curveBuilder.reset();
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
    chainingCallCount = 0;
    chainingAdvancement = 0.0f;
}

double V2VoiceRasterizer::getPhaseForTesting() const noexcept {
    return phase;
}

bool V2VoiceRasterizer::renderAudio(
    const V2RenderRequest& request,
    Buffer<float> output,
    V2RenderResult& result) noexcept {
    return renderBlock(request, output, result);
}

bool V2VoiceRasterizer::sampleArtifacts(
    const V2RasterArtifacts& artifacts,
    const V2RenderRequest& request,
    Buffer<float> out,
    V2RenderResult& result) noexcept {
    int wavePointCount = artifacts.waveX.size();
    Buffer<float> waveX = artifacts.waveX;
    Buffer<float> waveY = artifacts.waveY;
    Buffer<float> slopeUsed = artifacts.slope;
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(out.size());

    int currentIndex = jlimit(0, wavePointCount - 2, sampleIndex);
    double localPhase = phase;

    for (int i = 0; i < out.size(); ++i) {
        out[i] = V2WaveSampling::sampleAtPhase(localPhase, waveX, waveY, slopeUsed, wavePointCount, currentIndex);
        localPhase += deltaX;
    }

    // Legacy voice sampling keeps a running block spillover and only folds once.
    if (localPhase > 0.5) {
        localPhase -= 1.0;
    }

    phase = localPhase;
    sampleIndex = currentIndex;

    result.rendered = true;
    result.stillActive = true;
    result.samplesWritten = out.size();
    return true;
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

    if (! graph.buildInterceptArtifacts(workspace, interpolatorContext, positionerContext, artifacts)) {
        return false;
    }

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

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    if (! graph.buildCurveAndWaveArtifacts(
            workspace,
            static_cast<int>(artifacts.intercepts->size()),
            curveBuilderContext,
            waveBuilderContext,
            artifacts)) {
        return false;
    }

    return true;
}
