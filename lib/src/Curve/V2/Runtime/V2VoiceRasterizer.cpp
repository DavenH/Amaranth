#include "V2VoiceRasterizer.h"
#include "V2WaveSampling.h"

namespace {
constexpr double kMinCycleLength = 1e-9;

int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}
}

V2VoiceRasterizer::V2VoiceRasterizer() :
        graph(&interpolator, &chainingPositionerPipeline, &curveBuilder, &waveBuilder, &sampler) {
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
    result = V2RenderResult{};

    if (! workspace.isPrepared() || mesh == nullptr || output.empty() || ! request.isValid()) {
        return false;
    }

    int numSamples = clampRequestedSamples(request.numSamples, output);
    if (numSamples <= 0) {
        return false;
    }

    V2RasterArtifacts artifacts;
    if (! renderArtifacts(artifacts)) {
        return false;
    }

    Buffer<float> out = output.withSize(numSamples);
    int wavePointCount = artifacts.waveX.size();
    Buffer<float> waveX = artifacts.waveX;
    Buffer<float> waveY = artifacts.waveY;
    Buffer<float> slopeUsed = artifacts.slope;
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(numSamples);

    int currentIndex = jlimit(0, wavePointCount - 2, sampleIndex);
    double localPhase = phase;
    double cycleStart = cycleStartX;
    double cycleEnd = cycleEndX;
    double cycleLength = cycleEnd - cycleStart;
    bool wraps = cycleLength > kMinCycleLength;

    for (int i = 0; i < out.size(); ++i) {
        if (wraps) {
            while (localPhase >= cycleEnd) {
                localPhase -= cycleLength;
            }
            while (localPhase < cycleStart) {
                localPhase += cycleLength;
            }
        }

        out[i] = V2WaveSampling::sampleAtPhase(localPhase, waveX, waveY, slopeUsed, wavePointCount, currentIndex);
        localPhase += deltaX;
    }

    if (wraps) {
        while (localPhase >= cycleEnd) {
            localPhase -= cycleLength;
        }
        while (localPhase < cycleStart) {
            localPhase += cycleLength;
        }
    }

    phase = localPhase;
    sampleIndex = currentIndex;

    result.rendered = true;
    result.stillActive = true;
    result.samplesWritten = out.size();
    return true;
}

bool V2VoiceRasterizer::renderIntercepts(V2RasterArtifacts& artifacts) noexcept {
    artifacts.clear();
    if (! workspace.isPrepared() || mesh == nullptr) {
        return false;
    }

    graph.setPositioner(static_cast<V2PositionerStage*>(&chainingPositionerPipeline));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    if (chainingCallCount > 0 && chainingAdvancement > 0.0f) {
        interpolatorContext.morph.time.setValueDirect(jmin(
            1.0f,
            interpolatorContext.morph.time.getCurrentValue() + chainingAdvancement));
    }
    V2PositionerContext positionerContext = makePositionerContext(controls);
    positionerContext.cyclic = true;

    int outCount = 0;
    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, outCount)) {
        return false;
    }

    if (chainingCallCount == 0 && controls.minLineLength > 0.0f) {
        chainingAdvancement = controls.minLineLength * 1.1f;
    }

    ++chainingCallCount;
    artifacts.intercepts = &workspace.intercepts;
    return outCount > 0;
}

bool V2VoiceRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    waveBuilderContext.componentPath.deformRegions = &workspace.deformRegions;

    int curveCount = 0;
    if (! curveBuilder.run(
            workspace.intercepts,
            static_cast<int>(artifacts.intercepts->size()),
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    int zeroIndex = 0;
    int oneIndex = 0;
    int wavePointCount = 0;
    if (! waveBuilder.run(
            workspace.curves,
            curveCount,
            workspace.waveX,
            workspace.waveY,
            workspace.diffX,
            workspace.slope,
            wavePointCount,
            zeroIndex,
            oneIndex,
            waveBuilderContext)) {
        return false;
    }

    artifacts.curves = &workspace.curves;
    artifacts.waveX = workspace.waveX.withSize(wavePointCount);
    artifacts.waveY = workspace.waveY.withSize(wavePointCount);
    artifacts.diffX = workspace.diffX.withSize(jmax(0, wavePointCount - 1));
    artifacts.slope = workspace.slope.withSize(jmax(0, wavePointCount - 1));
    artifacts.deformRegions = &workspace.deformRegions;
    artifacts.zeroIndex = zeroIndex;
    artifacts.oneIndex = oneIndex;

    cycleStartX = controls.minX;
    cycleEndX = controls.maxX;

    return wavePointCount > 1;
}
