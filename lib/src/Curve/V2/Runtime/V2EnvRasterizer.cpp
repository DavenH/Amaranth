#include "V2EnvRasterizer.h"

namespace {
constexpr double kMinLoopLength = 1e-6;

int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}
}

V2EnvRasterizer::V2EnvRasterizer() :
        graph(&interpolator, &linearPositioner, &curveBuilder, &waveBuilder, &sampler)
{}

void V2EnvRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
}

void V2EnvRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
}

void V2EnvRasterizer::updateControlData(const V2EnvControlSnapshot& snapshot) noexcept {
    controls = snapshot;
}

void V2EnvRasterizer::noteOn() noexcept {
    envState.noteOn();
    samplePosition = 0.0;
    sampleIndex = 0;
}

bool V2EnvRasterizer::noteOff() noexcept {
    return envState.noteOff(controls.hasReleaseCurve);
}

bool V2EnvRasterizer::transitionToLooping(bool canLoop, bool overextends) noexcept {
    return envState.transitionToLooping(canLoop, overextends);
}

bool V2EnvRasterizer::consumeReleaseTrigger() noexcept {
    return envState.consumeReleaseTrigger();
}

V2EnvMode V2EnvRasterizer::getMode() const noexcept {
    return envState.getMode();
}

bool V2EnvRasterizer::isReleasePending() const noexcept {
    return envState.isReleasePending();
}

double V2EnvRasterizer::getSamplePositionForTesting() const noexcept {
    return samplePosition;
}

bool V2EnvRasterizer::renderAudio(
    const V2RenderRequest& request,
    Buffer<float> output,
    V2RenderResult& result) noexcept {
    result = V2RenderResult{};

    if (! workspace.isPrepared()
            || output.empty()
            || ! request.isValid()
            || (mesh == nullptr && testInterpolator == nullptr)) {
        return false;
    }

    int numSamples = clampRequestedSamples(request.numSamples, output);
    if (numSamples <= 0) {
        return false;
    }

    int wavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    if (! buildWaveForCurrentState(request, numSamples, wavePointCount, zeroIndex, oneIndex)) {
        return false;
    }

    Buffer<float> out = output.withSize(numSamples);
    Buffer<float> waveX = workspace.waveX.withSize(wavePointCount);
    Buffer<float> waveY = workspace.waveY.withSize(wavePointCount);
    Buffer<float> slopeUsed = workspace.slope.withSize(wavePointCount - 1);

    int currentIndex = jlimit(0, wavePointCount - 2, sampleIndex);
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(numSamples);

    bool haveLoop = controls.hasLoopRegion && controls.loopEndX > controls.loopStartX + static_cast<float>(kMinLoopLength);
    double loopStart = controls.loopStartX;
    double loopEnd = controls.loopEndX;
    double loopLength = loopEnd - loopStart;

    if (envState.consumeReleaseTrigger() && controls.hasReleaseCurve) {
        samplePosition = controls.releaseStartX;
        currentIndex = 0;
    }

    for (int i = 0; i < out.size(); ++i) {
        double phase = samplePosition;

        if (envState.getMode() == V2EnvMode::Looping && haveLoop) {
            while (phase >= loopEnd) {
                phase -= loopLength;
            }
            while (phase < loopStart) {
                phase += loopLength;
            }
        }

        out[i] = sampleAtPhase(phase, waveX, waveY, slopeUsed, wavePointCount, currentIndex);
        samplePosition = phase + deltaX;

        if (envState.getMode() == V2EnvMode::Looping && haveLoop) {
            while (samplePosition >= loopEnd) {
                samplePosition -= loopLength;
            }
        }
    }

    sampleIndex = currentIndex;
    result.rendered = true;
    result.stillActive = true;
    result.samplesWritten = out.size();
    return true;
}

void V2EnvRasterizer::setInterpolatorForTesting(V2InterpolatorStage* stage) noexcept {
    testInterpolator = stage;
}

bool V2EnvRasterizer::buildWaveForCurrentState(
    const V2RenderRequest&,
    int,
    int& wavePointCount,
    int& zeroIndex,
    int& oneIndex) noexcept {
    graph.setInterpolator(testInterpolator != nullptr ? testInterpolator : static_cast<V2InterpolatorStage*>(&interpolator));
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

    return wavePointCount > 1;
}

float V2EnvRasterizer::sampleAtPhase(
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
