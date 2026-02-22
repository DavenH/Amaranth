#pragma once

#include <vector>

#include "../../Curve.h"
#include "../../Intercept.h"
#include "../../Mesh.h"
#include "../../MeshRasterizer.h"
#include "../../../Obj/MorphPosition.h"
#include "../Runtime/V2RenderTypes.h"

struct V2InterpolatorContext {
    const Mesh* mesh{nullptr};
    MorphPosition morph{};
    bool wrapPhases{false};
    int primaryDimension{Vertex::Time};

    V2InterpolatorContext() = default;

    V2InterpolatorContext(
        const Mesh* mesh,
        const MorphPosition& morph,
        bool wrapPhases,
        int primaryDimension = Vertex::Time) noexcept :
            mesh(mesh)
        ,   morph(morph)
        ,   wrapPhases(wrapPhases)
        ,   primaryDimension(primaryDimension)
    {}
};

struct V2PositionerContext {
    MeshRasterizer::ScalingType scaling{MeshRasterizer::Unipolar};
    bool cyclic{false};
    float padding{0.0f};
    float minX{0.0f};
    float maxX{1.0f};

    V2PositionerContext() = default;

    V2PositionerContext(
        MeshRasterizer::ScalingType scaling,
        bool cyclic,
        float minX,
        float maxX,
        float padding = 0.0f) noexcept :
            scaling(scaling)
        ,   cyclic(cyclic)
        ,   padding(padding)
        ,   minX(minX)
        ,   maxX(maxX)
    {}
};

struct V2CurveBuilderContext {
    enum class PaddingPolicy {
        Generic,
        FxLegacyFixed
    };

    MeshRasterizer::ScalingType scaling{MeshRasterizer::Unipolar};
    int paddingCount{2};
    PaddingPolicy paddingPolicy{PaddingPolicy::Generic};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};

    V2CurveBuilderContext() = default;

    V2CurveBuilderContext(
        MeshRasterizer::ScalingType scaling,
        bool interpolateCurves,
        bool lowResolution,
        bool integralSampling,
        PaddingPolicy paddingPolicy = PaddingPolicy::Generic,
        int paddingCount = 2) noexcept :
            scaling(scaling)
        ,   paddingCount(paddingCount)
        ,   paddingPolicy(paddingPolicy)
        ,   interpolateCurves(interpolateCurves)
        ,   lowResolution(lowResolution)
        ,   integralSampling(integralSampling)
    {}
};

struct V2WaveBuilderContext {
    bool interpolateCurves{true};

    V2WaveBuilderContext() = default;

    explicit V2WaveBuilderContext(bool interpolateCurves) noexcept :
            interpolateCurves(interpolateCurves)
    {}
};

struct V2SamplerContext {
    V2RenderRequest request{};
    int wavePointCount{0};
    int zeroIndex{0};
    int oneIndex{0};

    V2SamplerContext() = default;

    V2SamplerContext(
        const V2RenderRequest& request,
        int wavePointCount = 0,
        int zeroIndex = 0,
        int oneIndex = 0) noexcept :
            request(request)
        ,   wavePointCount(wavePointCount)
        ,   zeroIndex(zeroIndex)
        ,   oneIndex(oneIndex)
    {}
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
        int& outWavePointCount,
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
