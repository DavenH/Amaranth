#include "V2EnvRasterizer.h"
#include "V2WaveSampling.h"

#include <algorithm>

namespace {
constexpr double kMinLoopLength = 1e-6;
constexpr int kLoopMinSizeIcpts = 1;

int clampRequestedSamples(int requested, Buffer<float> output) {
    if (requested <= 0 || output.empty()) {
        return 0;
    }

    return jmin(requested, output.size());
}

bool isLoopable(int loopIndex, int sustainIndex) {
    return loopIndex >= 0 && sustainIndex >= 0 && sustainIndex - loopIndex >= kLoopMinSizeIcpts;
}

float getLoopLength(const std::vector<Intercept>& intercepts, int loopIndex, int sustainIndex) {
    if (loopIndex < 0
            || sustainIndex < 0
            || loopIndex >= static_cast<int>(intercepts.size())
            || sustainIndex >= static_cast<int>(intercepts.size())
            || sustainIndex <= loopIndex) {
        return -1.0f;
    }

    return intercepts[sustainIndex].x - intercepts[loopIndex].x;
}

void evaluateLoopSustainIndices(
    const std::vector<Intercept>& intercepts,
    const V2EnvControlSnapshot& controls,
    int& loopIndex,
    int& sustainIndex) {
    loopIndex = -1;
    sustainIndex = -1;

    if (intercepts.empty()) {
        return;
    }

    const int end = static_cast<int>(intercepts.size()) - 1;
    if (! controls.hasLoopRegion) {
        sustainIndex = end;
        return;
    }

    for (int i = 0; i < static_cast<int>(intercepts.size()); ++i) {
        if (intercepts[i].x <= controls.loopStartX) {
            loopIndex = i;
        }

        if (intercepts[i].x <= controls.loopEndX) {
            sustainIndex = i;
        }
    }

    if (sustainIndex < 0) {
        sustainIndex = end;
    }

    if (loopIndex >= 0 && sustainIndex - loopIndex < kLoopMinSizeIcpts) {
        loopIndex = -1;
    }
}

void processEnvIntercepts(
    std::vector<Intercept>& intercepts,
    int& interceptCount,
    const V2EnvControlSnapshot& controls,
    int sustainIndex) {
    if (controls.scaling == MeshRasterizer::Bipolar) {
        return;
    }

    if (sustainIndex < 0 || sustainIndex >= interceptCount || sustainIndex == interceptCount - 1) {
        return;
    }

    Intercept sustain = intercepts[sustainIndex];
    sustain.cube = nullptr;
    sustain.x += 0.0001f;
    sustain.adjustedX = sustain.x;
    sustain.shp = 1.0f;

    if (sustain.y < 0.5f) {
        sustain.y = 0.5f;
    }

    intercepts.emplace_back(sustain);
    std::sort(intercepts.begin(), intercepts.end());
    interceptCount = static_cast<int>(intercepts.size());
}

void applyResolutionPolicy(std::vector<Curve>& curves, const V2CurveBuilderContext& context) {
    if (curves.size() < 3) {
        return;
    }

    if (context.lowResolution && curves.size() > 8) {
        for (auto& curve : curves) {
            curve.resIndex = Curve::resolutions - 1;
            curve.setShouldInterpolate(false);
        }
        return;
    }

    for (auto& curve : curves) {
        curve.setShouldInterpolate(! context.lowResolution && context.interpolateCurves);
    }

    float baseFactor = context.lowResolution ? 0.4f : context.integralSampling ? 0.05f : 0.1f;
    float base = baseFactor / static_cast<float>(Curve::resolution);

    for (int i = 1; i < static_cast<int>(curves.size()) - 1; ++i) {
        float dx = curves[i + 1].c.x - curves[i - 1].a.x;

        for (int j = 0; j < Curve::resolutions; ++j) {
            int res = Curve::resolution >> j;
            if (dx < base * static_cast<float>(res)) {
                curves[i].resIndex = j;
            }
        }
    }

    const int padding = 2;
    int lastIdx = static_cast<int>(curves.size()) - 1;
    curves.front().resIndex = curves[lastIdx - 2 * (padding - 1)].resIndex;
    curves.back().resIndex = curves[2 * padding - 1].resIndex;
}

bool buildEnvCurves(
    const std::vector<Intercept>& intercepts,
    int interceptCount,
    V2EnvMode mode,
    const V2EnvControlSnapshot& controls,
    int loopIndex,
    int sustainIndex,
    std::vector<Curve>& outCurves,
    int& outCurveCount,
    const V2CurveBuilderContext& curveContext) {
    outCurves.clear();
    outCurveCount = 0;

    const int end = interceptCount - 1;
    if (end <= 0) {
        return false;
    }

    if (Curve::table == nullptr) {
        Curve::calcTable();
    }

    const int numLoopPoints = sustainIndex - loopIndex;
    const bool loopable = mode != V2EnvMode::Releasing
        && isLoopable(loopIndex, sustainIndex)
        && numLoopPoints > 1;
    const float loopLength = loopable ? getLoopLength(intercepts, loopIndex, sustainIndex) : 0.0f;

    if (mode == V2EnvMode::Looping && loopable) {
        std::vector<Intercept> loopIcpts;
        loopIcpts.reserve(static_cast<size_t>((sustainIndex - loopIndex + 1) + 4));

        loopIcpts.emplace_back(intercepts[sustainIndex - 2]);
        loopIcpts.emplace_back(intercepts[sustainIndex - 1]);
        loopIcpts[0].x -= loopLength;
        loopIcpts[1].x -= loopLength;

        for (int i = loopIndex; i <= sustainIndex; ++i) {
            loopIcpts.emplace_back(intercepts[i]);
        }

        Intercept loopBackA(intercepts[loopIndex + 1]);
        Intercept loopBackB(intercepts[loopIndex + 2]);
        loopBackA.x += loopLength;
        loopBackB.x += loopLength;
        loopIcpts.emplace_back(loopBackA);
        loopIcpts.emplace_back(loopBackB);

        outCurves.reserve(loopIcpts.size());
        for (int i = 0; i < static_cast<int>(loopIcpts.size()) - 2; ++i) {
            outCurves.emplace_back(loopIcpts[i], loopIcpts[i + 1], loopIcpts[i + 2]);
        }
    } else {
        Intercept front1(-0.05f, intercepts[0].y);
        Intercept front2(-0.075f, intercepts[0].y);
        Intercept front3(-0.1f, intercepts[0].y);
        Intercept back1;
        Intercept back2;
        Intercept back3;

        const Intercept& endIcpt = intercepts[end];

        if (mode == V2EnvMode::Releasing) {
            back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
            back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
            back3 = Intercept(endIcpt.x + 0.003f, endIcpt.y);
        } else if (loopable) {
            back1 = Intercept(intercepts[loopIndex + 1]);
            back2 = Intercept(intercepts[loopIndex + 2]);
            back1.x += loopLength;
            back2.x += loopLength;
        } else {
            back1 = Intercept(endIcpt.x + 0.001f, endIcpt.y);
            back2 = Intercept(endIcpt.x + 0.002f, endIcpt.y);
            back3 = Intercept(endIcpt.x + 2.5f, endIcpt.y);
        }

        outCurves.reserve(static_cast<size_t>(interceptCount + 6));
        outCurves.emplace_back(front3, front2, front1);
        outCurves.emplace_back(front2, front1, intercepts[0]);
        outCurves.emplace_back(front1, intercepts[0], intercepts[1]);

        for (int i = 0; i < interceptCount - 2; ++i) {
            outCurves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
        }

        outCurves.emplace_back(intercepts[end - 1], intercepts[end], back1);
        outCurves.emplace_back(intercepts[end], back1, back2);
        if (mode == V2EnvMode::Releasing || ! loopable) {
            outCurves.emplace_back(back1, back2, back3);
        }
    }

    applyResolutionPolicy(outCurves, curveContext);
    for (auto& curve : outCurves) {
        curve.recalculateCurve();
    }

    outCurveCount = static_cast<int>(outCurves.size());
    return outCurveCount > 0;
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

    if (envState.getMode() == V2EnvMode::Looping) {
        currentIndex = jlimit(0, wavePointCount - 2, zeroIndex);
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
    graph.setPositioner(static_cast<V2PositionerStage*>(&linearPositioner));

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext(
        controls.scaling,
        false,
        controls.minX,
        controls.maxX);

    int interceptCount = 0;
    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    int loopIndex = -1;
    int sustainIndex = -1;
    evaluateLoopSustainIndices(workspace.intercepts, controls, loopIndex, sustainIndex);
    processEnvIntercepts(workspace.intercepts, interceptCount, controls, sustainIndex);

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    int curveCount = 0;
    if (! buildEnvCurves(
            workspace.intercepts,
            interceptCount,
            envState.getMode(),
            controls,
            loopIndex,
            sustainIndex,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    V2WaveBuilderContext waveBuilderContext = makeWaveBuilderContext(controls);
    return waveBuilder.run(
        workspace.curves,
        curveCount,
        workspace.waveX,
        workspace.waveY,
        workspace.diffX,
        workspace.slope,
        wavePointCount,
        zeroIndex,
        oneIndex,
        waveBuilderContext);
}
