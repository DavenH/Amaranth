#pragma once

#include <vector>

#include "../../Curve.h"
#include "../../Intercept.h"
#include "../../Mesh.h"
#include "../../../Obj/MorphPosition.h"
#include "../Runtime/V2RenderTypes.h"

struct V2InterpolatorContext {
    const Mesh* mesh{nullptr};
    MorphPosition morph{};
    bool wrapPhases{false};
};

struct V2PositionerContext {
    V2ScalingType scaling{V2ScalingType::Unipolar};
    bool cyclic{false};
    float padding{0.0f};
    float minX{0.0f};
    float maxX{1.0f};
};

struct V2CurveBuilderContext {
    V2ScalingType scaling{V2ScalingType::Unipolar};
    int paddingCount{2};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};
};

struct V2WaveBuilderContext {
    bool interpolateCurves{true};
};

struct V2SamplerContext {
    V2RenderRequest request{};
    int zeroIndex{0};
    int oneIndex{0};
};

class V2InterpolatorStage {
public:
    virtual ~V2InterpolatorStage() = default;

    virtual bool run(
        const V2InterpolatorContext& context,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept = 0;
};

class V2PositionerStage {
public:
    virtual ~V2PositionerStage() = default;

    virtual bool run(
        std::vector<Intercept>& ioIntercepts,
        int& ioCount,
        const V2PositionerContext& context) noexcept = 0;
};

class V2CurveBuilderStage {
public:
    virtual ~V2CurveBuilderStage() = default;

    virtual bool run(
        const std::vector<Intercept>& inIntercepts,
        int inCount,
        std::vector<Curve>& outCurves,
        int& outCount,
        const V2CurveBuilderContext& context) noexcept = 0;
};

class V2WaveBuilderStage {
public:
    virtual ~V2WaveBuilderStage() = default;

    virtual bool run(
        const std::vector<Curve>& curves,
        int curveCount,
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> diffX,
        Buffer<float> slope,
        int& zeroIndex,
        int& oneIndex,
        const V2WaveBuilderContext& context) noexcept = 0;
};

class V2SamplerStage {
public:
    virtual ~V2SamplerStage() = default;

    virtual V2RenderResult run(
        Buffer<float> waveX,
        Buffer<float> waveY,
        Buffer<float> slope,
        Buffer<float> output,
        const V2SamplerContext& context) noexcept = 0;
};

