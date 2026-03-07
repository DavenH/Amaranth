#pragma once

#include <vector>

#include <Curve/Curve.h>
#include <Curve/IDeformer.h>
#include <Curve/Intercept.h>
#include <Curve/Mesh.h>
#include <Obj/MorphPosition.h>
#include <Curve/V2/Core/V2RenderTypes.h>

struct V2PathContext {
    IDeformer* path{nullptr};
    int noiseSeed{-1};
    const short* vertOffsetSeeds{nullptr};
    const short* phaseOffsetSeeds{nullptr};
    bool enabled{false};

    V2PathContext() = default;

    V2PathContext(
        IDeformer* path,
        int noiseSeed,
        const short* vertOffsetSeeds,
        const short* phaseOffsetSeeds,
        bool enabled) noexcept :
            path(path)
        ,   noiseSeed(noiseSeed)
        ,   vertOffsetSeeds(vertOffsetSeeds)
        ,   phaseOffsetSeeds(phaseOffsetSeeds)
        ,   enabled(enabled)
    {}
};

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
    struct PointPathContext :
            public V2PathContext {
        bool noOffsetAtEnds{false};
        bool useLegacyTimeProgress{true};

        PointPathContext() = default;

        PointPathContext(
            IDeformer* path,
            int noiseSeed,
            const short* vertOffsetSeeds,
            const short* phaseOffsetSeeds,
            bool enabled,
            bool noOffsetAtEnds = false,
            bool useLegacyTimeProgress = true) noexcept :
                V2PathContext(path, noiseSeed, vertOffsetSeeds, phaseOffsetSeeds, enabled)
            ,   noOffsetAtEnds(noOffsetAtEnds)
            ,   useLegacyTimeProgress(useLegacyTimeProgress)
        {}
    };

    V2ScalingType scaling{V2ScalingType::Unipolar};
    bool cyclic{false};
    float padding{0.0f};
    float minX{0.0f};
    float maxX{1.0f};
    int interpolationDimension{Vertex::Time};
    MorphPosition morph{};
    PointPathContext pointPath{};

    V2PositionerContext() = default;

    V2PositionerContext(
        V2ScalingType scaling,
        bool cyclic,
        float minX,
        float maxX,
        float padding,
        int interpolationDimension,
        const MorphPosition& morph,
        const PointPathContext& pointPath) noexcept :
            scaling(scaling)
        ,   cyclic(cyclic)
        ,   padding(padding)
        ,   minX(minX)
        ,   maxX(maxX)
        ,   interpolationDimension(interpolationDimension)
        ,   morph(morph)
        ,   pointPath(pointPath)
    {}

    V2PositionerContext(
        V2ScalingType scaling,
        bool cyclic,
        float minX,
        float maxX,
        float padding = 0.0f) noexcept :
            V2PositionerContext(
                scaling,
                cyclic,
                minX,
                maxX,
                padding,
                Vertex::Time,
                MorphPosition(),
                PointPathContext())
    {}
};

struct V2CurveBuilderContext {
    enum class PaddingPolicy {
        Generic,
        FxLegacyFixed
    };

    V2ScalingType scaling{V2ScalingType::Unipolar};
    int paddingCount{2};
    PaddingPolicy paddingPolicy{PaddingPolicy::Generic};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};

    V2CurveBuilderContext() = default;

    V2CurveBuilderContext(
        V2ScalingType scaling,
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
    struct ComponentPathContext :
            public V2PathContext {
        bool decoupled{false};
        bool lowResolution{false};
        float morphTime{0.0f};
        int decoupledPhaseOffsetSeed{0};
        int decoupledVertOffsetSeed{0};
        std::vector<V2DeformRegion>* deformRegions{nullptr};

        ComponentPathContext() = default;

        ComponentPathContext(
            IDeformer* path,
            int noiseSeed,
            const short* vertOffsetSeeds,
            const short* phaseOffsetSeeds,
            bool enabled,
            bool decoupled,
            bool lowResolution,
            float morphTime,
            int decoupledPhaseOffsetSeed = 0,
            int decoupledVertOffsetSeed = 0,
            std::vector<V2DeformRegion>* deformRegions = nullptr) noexcept :
                V2PathContext(path, noiseSeed, vertOffsetSeeds, phaseOffsetSeeds, enabled)
            ,   decoupled(decoupled)
            ,   lowResolution(lowResolution)
            ,   morphTime(morphTime)
            ,   decoupledPhaseOffsetSeed(decoupledPhaseOffsetSeed)
            ,   decoupledVertOffsetSeed(decoupledVertOffsetSeed)
            ,   deformRegions(deformRegions)
        {}
    };

    bool interpolateCurves{true};
    ComponentPathContext componentPath{};

    V2WaveBuilderContext() = default;

    explicit V2WaveBuilderContext(bool interpolateCurves) noexcept :
            interpolateCurves(interpolateCurves)
    {}

    V2WaveBuilderContext(
        bool interpolateCurves,
        const ComponentPathContext& componentPath) noexcept :
            interpolateCurves(interpolateCurves)
        ,   componentPath(componentPath)
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

    virtual void reset() noexcept {}

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
            V2WaveBuffers waveBuffers,
            int& outWavePointCount,
            int& zeroIndex,
            int& oneIndex,
            const V2WaveBuilderContext& context) noexcept = 0;
};

