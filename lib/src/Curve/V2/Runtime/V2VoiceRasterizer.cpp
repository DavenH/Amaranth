#include "V2VoiceRasterizer.h"

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
        graph(&interpolator, &linearPositioner, &curveBuilder, &waveBuilder, &sampler)
{}

void V2VoiceRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
}

void V2VoiceRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
}

void V2VoiceRasterizer::updateControlData(const V2VoiceControlSnapshot& snapshot) noexcept {
    controls = snapshot;
}

void V2VoiceRasterizer::resetPhase(double phase) noexcept {
    this->phase = phase;
    sampleIndex = 0;
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

    int wavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    if (! buildWave(wavePointCount, zeroIndex, oneIndex)) {
        return false;
    }

    Buffer<float> out = output.withSize(numSamples);
    Buffer<float> waveX = workspace.waveX.withSize(wavePointCount);
    Buffer<float> waveY = workspace.waveY.withSize(wavePointCount);
    Buffer<float> slopeUsed = workspace.slope.withSize(wavePointCount - 1);
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(numSamples);

    int currentIndex = jlimit(0, wavePointCount - 2, sampleIndex);
    double localPhase = phase;
    double cycleStart = cycleStartX;
    double cycleEnd = cycleEndX;
    double cycleLength = cycleEnd - cycleStart;
    bool wraps = controls.cyclic && cycleLength > kMinCycleLength;

    for (int i = 0; i < out.size(); ++i) {
        if (wraps) {
            while (localPhase >= cycleEnd) {
                localPhase -= cycleLength;
            }
            while (localPhase < cycleStart) {
                localPhase += cycleLength;
            }
        }

        out[i] = sampleAtPhase(localPhase, waveX, waveY, slopeUsed, wavePointCount, currentIndex);
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

bool V2VoiceRasterizer::buildWave(int& wavePointCount, int& zeroIndex, int& oneIndex) noexcept {
    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                        : static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext;
    interpolatorContext.mesh = mesh;
    interpolatorContext.morph = controls.morph;
    interpolatorContext.wrapPhases = controls.wrapPhases;

    V2PositionerContext positionerContext;
    positionerContext.scaling = controls.scaling;
    positionerContext.cyclic = controls.cyclic;
    positionerContext.minX = controls.minX;
    positionerContext.maxX = controls.maxX;

    int interceptCount = 0;
    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.scaling = controls.scaling;
    curveBuilderContext.interpolateCurves = controls.interpolateCurves;
    curveBuilderContext.lowResolution = controls.lowResolution;
    curveBuilderContext.integralSampling = controls.integralSampling;

    int curveCount = 0;
    if (! curveBuilder.run(
            workspace.intercepts,
            interceptCount,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = controls.interpolateCurves;
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

    if (wavePointCount <= 1) {
        return false;
    }

    const int clampedZero = jlimit(0, wavePointCount - 2, zeroIndex);
    const int clampedOne = jlimit(clampedZero, wavePointCount - 2, oneIndex);
    cycleStartX = workspace.waveX[clampedZero];
    cycleEndX = workspace.waveX[jmin(clampedOne + 1, wavePointCount - 1)];

    if (cycleEndX <= cycleStartX + static_cast<float>(kMinCycleLength)) {
        cycleStartX = controls.minX;
        cycleEndX = controls.maxX;
    }

    return true;
}

float V2VoiceRasterizer::sampleAtPhase(
    double phase,
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> slope,
    int wavePointCount,
    int& ioSampleIndex) const noexcept {
    while (ioSampleIndex < wavePointCount - 2 && phase >= waveX[ioSampleIndex + 1]) {
        ++ioSampleIndex;
    }

    while (ioSampleIndex > 0 && phase < waveX[ioSampleIndex]) {
        --ioSampleIndex;
    }

    float sampled = 0.0f;
    if (phase <= waveX[0]) {
        sampled = waveY[0];
    } else if (phase >= waveX[wavePointCount - 1]) {
        sampled = waveY[wavePointCount - 1];
    } else {
        sampled = static_cast<float>((phase - waveX[ioSampleIndex]) * slope[ioSampleIndex] + waveY[ioSampleIndex]);
    }

    if (sampled != sampled) {
        return 0.0f;
    }

    return sampled;
}
