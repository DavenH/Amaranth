#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/V2/Runtime/V2GraphicRasterizer.h"
#include "../src/Curve/VertCube.h"

namespace RasterizerParityHarness {

using CubeVertexSpecs = std::array<Vertex, VertCube::numVerts>;

struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }

    Mesh mesh;
};

inline CubeVertexSpecs makeCubeFromPoles(
    float phaseOffset,
    float phaseTimeDelta,
    float phaseRedDelta,
    float phaseBlueDelta,
    float ampBase,
    float ampRedDelta,
    float ampBlueDelta,
    float curveValue) {
    CubeVertexSpecs specs{};

    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        bool timePole = false;
        bool redPole = false;
        bool bluePole = false;
        VertCube::getPoles(i, timePole, redPole, bluePole);

        Vertex spec;
        spec.values[Vertex::Time] = timePole ? 1.0f : 0.0f;
        spec.values[Vertex::Red] = redPole ? 1.0f : 0.0f;
        spec.values[Vertex::Blue] = bluePole ? 1.0f : 0.0f;
        spec.values[Vertex::Phase] = phaseOffset
            + (timePole ? phaseTimeDelta : 0.0f)
            + (redPole ? phaseRedDelta : 0.0f)
            + (bluePole ? phaseBlueDelta : 0.0f);
        spec.values[Vertex::Amp] = ampBase
            + (redPole ? ampRedDelta : 0.0f)
            + (bluePole ? ampBlueDelta : 0.0f);
        spec.values[Vertex::Curve] = curveValue;

        specs[i] = spec;
    }

    return specs;
}

inline void appendCube(Mesh& mesh, const CubeVertexSpecs& specs) {
    auto* cube = new VertCube(&mesh);
    mesh.addCube(cube);

    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        auto* vert = cube->lineVerts[i];
        for (int d = 0; d < Vertex::numElements; ++d) {
            vert->values[d] = specs[i].values[d];
        }
    }
}

struct GraphicRenderConfig {
    MorphPosition morph{0.25f, 0.5f, 0.75f};
    MeshRasterizer::ScalingType scaling{MeshRasterizer::Unipolar};

    bool cyclic{false};
    bool interpolateCurves{true};
    bool lowResolution{false};
    bool integralSampling{false};

    float minX{0.0f};
    float maxX{1.0f};
    int primaryDimension{Vertex::Time};

    int sampleCount{128};

    V2CapacitySpec capacities{128, 256, 4096, 0};
};

struct GraphicRunArtifacts {
    bool rendered{false};
    int zeroIndex{0};
    int oneIndex{0};

    std::vector<Intercept> intercepts;
    int curveCount{0};

    std::vector<float> waveX;
    std::vector<float> waveY;
    std::vector<float> diffX;
    std::vector<float> slope;

    std::vector<float> samples;
};

struct GraphicParityResult {
    GraphicRunArtifacts legacy;
    GraphicRunArtifacts v2;

    std::vector<float> sampleDiff;
    float maxAbsDiff{0.0f};
    float l2Diff{0.0f};
};

inline std::vector<float> toStdVector(Buffer<float> buffer) {
    std::vector<float> values(static_cast<size_t>(buffer.size()));
    for (int i = 0; i < buffer.size(); ++i) {
        values[static_cast<size_t>(i)] = buffer[i];
    }

    return values;
}

inline void computeDiff(GraphicParityResult& result) {
    int size = jmin(
        static_cast<int>(result.legacy.samples.size()),
        static_cast<int>(result.v2.samples.size()));

    if (size <= 0) {
        result.sampleDiff.clear();
        result.maxAbsDiff = 0.0f;
        result.l2Diff = 0.0f;
        return;
    }

    result.sampleDiff.assign(static_cast<size_t>(size), 0.0f);

    float maxAbs = 0.0f;
    double l2Sum = 0.0;

    for (int i = 0; i < size; ++i) {
        float diff = result.legacy.samples[static_cast<size_t>(i)] - result.v2.samples[static_cast<size_t>(i)];
        float absDiff = std::fabs(diff);
        result.sampleDiff[static_cast<size_t>(i)] = diff;

        if (absDiff > maxAbs) {
            maxAbs = absDiff;
        }

        l2Sum += static_cast<double>(diff) * static_cast<double>(diff);
    }

    result.maxAbsDiff = maxAbs;
    result.l2Diff = static_cast<float>(std::sqrt(l2Sum));
}

class GraphicParityFacade {
public:
    GraphicParityFacade() : legacy("GraphicLegacyParityHarness") {}

    bool run(const Mesh& mesh, const GraphicRenderConfig& config, GraphicParityResult& outResult) {
        outResult = GraphicParityResult{};

        bool legacyOk = runLegacy(mesh, config, outResult.legacy);
        bool v2Ok = runV2(mesh, config, outResult.v2);

        computeDiff(outResult);
        return legacyOk && v2Ok;
    }

private:
    bool runLegacy(const Mesh& mesh, const GraphicRenderConfig& config, GraphicRunArtifacts& out) {
        legacy.reset();
        legacy.setMesh(const_cast<Mesh*>(&mesh));
        legacy.setMorphPosition(config.morph);
        legacy.setScalingMode(config.scaling);
        legacy.setWrapsEnds(config.cyclic);
        legacy.setInterpolatesCurves(config.interpolateCurves);
        legacy.setLowresCurves(config.lowResolution);
        legacy.setIntegralSampling(config.integralSampling);
        legacy.setLimits(config.minX, config.maxX);

        if (config.primaryDimension != Vertex::Time) {
            legacy.setToOverrideDim(true);
            legacy.setOverridingDim(config.primaryDimension);
        } else {
            legacy.setToOverrideDim(false);
        }

        legacy.calcCrossPoints();
        legacy.makeCopy();

        out.rendered = legacy.isSampleable();
        out.zeroIndex = legacy.getZeroIndex();
        out.oneIndex = legacy.getOneIndex();
        out.intercepts = legacy.getRastData().intercepts;
        out.curveCount = static_cast<int>(legacy.getCurves().size());
        out.waveX = toStdVector(legacy.getWaveX());
        out.waveY = toStdVector(legacy.getWaveY());
        out.diffX = toStdVector(legacy.getDiffX());
        out.slope = toStdVector(legacy.getSlopes());

        if (! out.rendered || config.sampleCount <= 0) {
            return false;
        }

        ScopedAlloc<float> sampleMemory(config.sampleCount);
        Buffer<float> samples = sampleMemory.withSize(config.sampleCount);
        int currentIndex = jlimit(0, jmax(0, legacy.getWaveX().size() - 2), legacy.getZeroIndex());

        double phase = 0.0;
        double delta = 1.0 / static_cast<double>(config.sampleCount);

        for (int i = 0; i < samples.size(); ++i) {
            samples[i] = legacy.sampleAt(phase, currentIndex);
            phase += delta;
        }

        out.samples = toStdVector(samples);
        return true;
    }

    bool runV2(const Mesh& mesh, const GraphicRenderConfig& config, GraphicRunArtifacts& out) {
        V2PrepareSpec prepareSpec;
        prepareSpec.capacities = config.capacities;
        prepareSpec.lowResolution = config.lowResolution;

        v2.prepare(prepareSpec);
        v2.setMeshSnapshot(&mesh);

        V2GraphicControlSnapshot controls;
        controls.morph = config.morph;
        controls.scaling = config.scaling;
        controls.cyclic = config.cyclic;
        controls.wrapPhases = config.cyclic;
        controls.interpolateCurves = config.interpolateCurves;
        controls.lowResolution = config.lowResolution;
        controls.integralSampling = config.integralSampling;
        controls.minX = config.minX;
        controls.maxX = config.maxX;
        controls.primaryDimension = config.primaryDimension;
        v2.updateControlData(controls);

        V2RasterArtifacts artifacts;
        if (! v2.renderArtifacts(artifacts)
                || artifacts.intercepts == nullptr
                || artifacts.curves == nullptr
                || artifacts.waveBuffers.waveX.size() <= 1) {
            return false;
        }

        out.rendered = true;
        out.zeroIndex = artifacts.zeroIndex;
        out.oneIndex = artifacts.oneIndex;
        out.intercepts = *artifacts.intercepts;
        out.curveCount = static_cast<int>(artifacts.curves->size());
        out.waveX = toStdVector(artifacts.waveBuffers.waveX);
        out.waveY = toStdVector(artifacts.waveBuffers.waveY);
        out.diffX = toStdVector(artifacts.waveBuffers.diffX);
        out.slope = toStdVector(artifacts.waveBuffers.slope);

        if (config.sampleCount <= 0) {
            return false;
        }

        ScopedAlloc<float> sampleMemory(config.sampleCount);
        Buffer<float> samples = sampleMemory.withSize(config.sampleCount);
        samples.zero();

        V2RenderRequest request;
        request.numSamples = config.sampleCount;

        V2RenderResult result;
        if (! v2.renderBlock(request, samples, result) || ! result.rendered) {
            return false;
        }

        out.samples = toStdVector(samples);
        return true;
    }

    MeshRasterizer legacy;
    V2GraphicRasterizer v2;
};

} // namespace RasterizerParityHarness
