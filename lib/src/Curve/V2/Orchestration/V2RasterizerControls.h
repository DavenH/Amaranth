#pragma once

#include "../Stages/V2StageInterfaces.h"

struct V2CommonControlSnapshot {
    MorphPosition morph{};
    V2ScalingType scaling{V2ScalingType::Unipolar};
    bool wrapPhases{false};
    bool cyclic{false};
    float minX{0.0f};
    float maxX{1.0f};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};
    V2PositionerContext::PointPathContext pointPath{};
    V2WaveBuilderContext::ComponentPathContext componentPath{};
};

struct V2GraphicControlSnapshot :
        public V2CommonControlSnapshot {
    int primaryDimension{Vertex::Time};
};

struct V2FxControlSnapshot :
        public V2CommonControlSnapshot {
};

struct V2VoiceControlSnapshot :
        public V2CommonControlSnapshot {
    float minLineLength{0.0f};

    V2VoiceControlSnapshot() noexcept {
        scaling = V2ScalingType::Bipolar;
        cyclic = true;
    }
};

struct V2EnvControlSnapshot :
        public V2CommonControlSnapshot {
    bool hasReleaseCurve{true};
    bool hasLoopRegion{false};
    float loopStartX{0.0f};
    float loopEndX{1.0f};
    float releaseStartX{0.0f};
};

inline V2InterpolatorContext makeInterpolatorContext(
    const Mesh* mesh,
    const V2CommonControlSnapshot& controls,
    int primaryDimension = Vertex::Time) noexcept {
    return V2InterpolatorContext(mesh, controls.morph, controls.wrapPhases, primaryDimension);
}

inline V2PositionerContext makePositionerContext(const V2CommonControlSnapshot& controls) noexcept {
    V2PositionerContext::PointPathContext pointPath = controls.pointPath;
    pointPath.enabled = pointPath.enabled
        && pointPath.path != nullptr
        && pointPath.vertOffsetSeeds != nullptr
        && pointPath.phaseOffsetSeeds != nullptr;

    return V2PositionerContext(
        controls.scaling,
        controls.cyclic,
        controls.minX,
        controls.maxX,
        0.0f,
        Vertex::Time,
        controls.morph,
        pointPath);
}

inline V2PositionerContext makePositionerContext(
    const V2CommonControlSnapshot& controls,
    int interpolationDimension) noexcept {
    V2PositionerContext::PointPathContext pointPath = controls.pointPath;
    pointPath.enabled = pointPath.enabled
        && pointPath.path != nullptr
        && pointPath.vertOffsetSeeds != nullptr
        && pointPath.phaseOffsetSeeds != nullptr;

    return V2PositionerContext(
        controls.scaling,
        controls.cyclic,
        controls.minX,
        controls.maxX,
        0.0f,
        interpolationDimension,
        controls.morph,
        pointPath);
}

inline V2CurveBuilderContext makeCurveBuilderContext(
    const V2CommonControlSnapshot& controls,
    bool interpolateCurves,
    bool lowResolution,
    V2CurveBuilderContext::PaddingPolicy paddingPolicy = V2CurveBuilderContext::PaddingPolicy::Generic) noexcept {
    return V2CurveBuilderContext(
        controls.scaling,
        interpolateCurves && controls.interpolateCurves,
        lowResolution || controls.lowResolution,
        controls.integralSampling,
        paddingPolicy);
}

inline V2CurveBuilderContext makeCurveBuilderContext(
    const V2CommonControlSnapshot& controls,
    V2CurveBuilderContext::PaddingPolicy paddingPolicy = V2CurveBuilderContext::PaddingPolicy::Generic) noexcept {
    return makeCurveBuilderContext(
        controls,
        controls.interpolateCurves,
        controls.lowResolution,
        paddingPolicy);
}

inline V2WaveBuilderContext makeWaveBuilderContext(
    const V2CommonControlSnapshot& controls,
    bool interpolateCurves) noexcept {
    V2WaveBuilderContext::ComponentPathContext componentPath = controls.componentPath;
    componentPath.enabled &= componentPath.path != nullptr
        && componentPath.vertOffsetSeeds != nullptr
        && componentPath.phaseOffsetSeeds != nullptr;

    return V2WaveBuilderContext(interpolateCurves && controls.interpolateCurves, componentPath);
}

inline V2WaveBuilderContext makeWaveBuilderContext(const V2CommonControlSnapshot& controls) noexcept {
    V2WaveBuilderContext::ComponentPathContext componentPath = controls.componentPath;
    componentPath.enabled &= componentPath.path != nullptr
        && componentPath.vertOffsetSeeds != nullptr
        && componentPath.phaseOffsetSeeds != nullptr;

    return V2WaveBuilderContext(controls.interpolateCurves, componentPath);
}
