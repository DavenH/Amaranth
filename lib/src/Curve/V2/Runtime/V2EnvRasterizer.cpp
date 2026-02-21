#include "V2EnvRasterizer.h"
#include "V2WaveSampling.h"

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

        out[i] = V2WaveSampling::sampleAtPhase(phase, waveX, waveY, slopeUsed, wavePointCount, currentIndex);
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

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext = makePositionerContext(controls);
    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    V2BuiltArtifacts artifacts;
    if (! graph.buildArtifacts(
            workspace,
            interpolatorContext,
            positionerContext,
            curveBuilderContext,
            waveBuilderContext,
            artifacts)) {
        return false;
    }

    wavePointCount = artifacts.wavePointCount;
    zeroIndex = artifacts.zeroIndex;
    oneIndex = artifacts.oneIndex;
    return true;
}
