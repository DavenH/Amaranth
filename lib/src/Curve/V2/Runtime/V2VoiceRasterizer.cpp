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
        graph(&interpolator, &linearPositioner, &curveBuilder, &waveBuilder, &sampler)
{}

void V2VoiceRasterizer::prepare(const V2PrepareSpec& spec) {
    workspace.prepare(spec.capacities);
    controls.lowResolution = spec.lowResolution;
    chainingPositioner.reset();
    curveBuilder.reset();
}

void V2VoiceRasterizer::setMeshSnapshot(const Mesh* meshSnapshot) noexcept {
    mesh = meshSnapshot;
    chainingPositioner.reset();
    curveBuilder.reset();
}

void V2VoiceRasterizer::updateControlData(const V2VoiceControlSnapshot& snapshot) noexcept {
    if (controls.cyclic != snapshot.cyclic) {
        chainingPositioner.reset();
        curveBuilder.reset();
    }

    controls = snapshot;
}

void V2VoiceRasterizer::resetPhase(double phase) noexcept {
    this->phase = phase;
    sampleIndex = 0;
    chainingPositioner.reset();
    curveBuilder.reset();
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

bool V2VoiceRasterizer::buildWave(int& wavePointCount, int& zeroIndex, int& oneIndex) noexcept {
    graph.setPositioner(controls.cyclic ? static_cast<V2PositionerStage*>(&chainingPositioner)
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
