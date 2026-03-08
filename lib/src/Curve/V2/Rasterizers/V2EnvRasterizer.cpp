#include "V2EnvRasterizer.h"
#include "../Sampling/V2WaveSampling.h"

#include <algorithm>

namespace {
constexpr double kMinLoopLength = 1e-6;
constexpr int kLoopMinSizeIcpts = 1;

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
    if (controls.scaling == V2ScalingType::Bipolar) {
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

V2EnvRasterizer::V2EnvRasterizer() {
    setInterpolator(&interpolator);
    setPositioner(&linearPositioner);
    setCurveBuilder(&curveBuilder);
    setWaveBuilder(&waveBuilder);
}

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

bool V2EnvRasterizer::sampleArtifacts(
        const V2RasterArtifacts& artifacts,
        const V2RenderRequest& request,
        Buffer<float> out,
        V2RenderResult& result) noexcept {
    int wavePointCount = artifacts.waveBuffers.waveX.size();
    double deltaX = request.deltaX > 0.0 ? request.deltaX : 1.0 / static_cast<double>(out.size());

    if (envState.consumeReleaseTrigger() && controls.hasReleaseCurve) {
        samplePosition = controls.releaseStartX;
        sampleIndex = 0;
    }

    if (envState.getMode() == V2EnvMode::Looping) {
        sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    }

    bool loopActive = envState.getMode() == V2EnvMode::Looping
        && controls.hasLoopRegion
        && controls.loopEndX > controls.loopStartX + static_cast<float>(kMinLoopLength);

    LoopingPhasePolicy policy(
        samplePosition, deltaX,
        controls.loopStartX, controls.loopEndX,
        loopActive);
    result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, out, sampleIndex);
    samplePosition = policy.phase;
    return result.rendered;
}

bool V2EnvRasterizer::renderIntercepts(V2RasterArtifacts& artifacts) noexcept {
    artifacts.clear();

    if (! workspace.isPrepared() || (mesh == nullptr && testInterpolator == nullptr)) {
        return false;
    }

    setInterpolator(testInterpolator != nullptr ? testInterpolator : static_cast<V2InterpolatorStage*>(&interpolator));
    setPositioner(&linearPositioner);

    V2InterpolatorContext interpolatorContext = makeInterpolatorContext(mesh, controls);
    V2PositionerContext positionerContext(
        controls.scaling,
        false,
        controls.minX,
        controls.maxX);

    int interceptCount = 0;
    if (! runInterceptStages(interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    int loopIndex = -1;
    int sustainIndex = -1;
    evaluateLoopSustainIndices(workspace.intercepts, controls, loopIndex, sustainIndex);
    processEnvIntercepts(workspace.intercepts, interceptCount, controls, sustainIndex);

    artifacts.intercepts = &workspace.intercepts;
    return interceptCount > 0;
}

bool V2EnvRasterizer::renderWaveform(V2RasterArtifacts& artifacts) noexcept {
    if (! workspace.isPrepared()
            || artifacts.intercepts != &workspace.intercepts
            || artifacts.intercepts->empty()) {
        return false;
    }

    int loopIndex = -1;
    int sustainIndex = -1;
    evaluateLoopSustainIndices(workspace.intercepts, controls, loopIndex, sustainIndex);

    V2CurveBuilderContext curveBuilderContext = makeCurveBuilderContext(controls);
    int curveCount = 0;
    if (! buildEnvCurves(
            workspace.intercepts,
            static_cast<int>(artifacts.intercepts->size()),
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
    return buildWaveArtifactsFromCurves(curveCount, waveBuilderContext, artifacts);
}

void V2EnvRasterizer::setInterpolatorForTesting(V2InterpolatorStage* stage) noexcept {
    testInterpolator = stage;
}
