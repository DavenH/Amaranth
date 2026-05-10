#include <cmath>
#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Curve.h"
#include "../src/Curve/Mesh.h"
#include "Support/LegacyMeshRasterizer.h"
#include "../src/Curve/Rasterization/MeshWaveformRasterizer.h"
#include "../src/Curve/Rasterization/Pipelines/MeshSlicePipeline.h"
#include "../src/Curve/Rasterization/Policies/Curves/CurveWaveformPreparationPolicy.h"
#include "../src/Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h"
#include "../src/Curve/Rasterization/Policies/Core/InterceptPolicies.h"
#include "../src/Curve/Rasterization/Policies/Curves/InterceptPaddingFlagPolicy.h"
#include "../src/Curve/Rasterization/Sampling/GuideCurveSampler.h"
#include "../src/Curve/Rasterization/Sampling/WaveformSampler.h"
#include "../src/Curve/Rasterization/Sources/MeshCubeSource.h"
#include "../src/Curve/VertCube.h"
#include "RasterizerCompare.h"

namespace {
    struct MeshDeleter {
        void operator()(Mesh* mesh) const {
            if (mesh != nullptr) {
                mesh->destroy();
                delete mesh;
            }
        }
    };

    struct CurveTableScope {
        CurveTableScope() {
            if (refCount++ == 0) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (--refCount == 0) {
                Curve::deleteTable();
            }
        }

        inline static int refCount = 0;
    };

    class ConstantGuideCurveProvider :
            public GuideCurveProvider {
    public:
        float getTableValue(int guideIndex, float, const GuideCurveProvider::NoiseContext&) override {
            return values[guideIndex];
        }

        void sampleDownAddNoise(int guideIndex, Buffer<float> dest, const GuideCurveProvider::NoiseContext&) override {
            dest.set(values[guideIndex]);
        }

        Buffer<Float32> getTable(int) override {
            return {};
        }

        int getTableDensity(int) override {
            return GuideCurveProvider::tableSize;
        }

        float values[128] {};
    };

    void setCubeAsConstantPoint(
            VertCube* cube,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            bool time, red, blue;
            VertCube::getPoles(i, time, red, blue);

            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Time]  = time ? 1.f : 0.f;
            vertex->values[Vertex::Red]   = red  ? 1.f : 0.f;
            vertex->values[Vertex::Blue]  = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp]   = time ? highAmp   : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    std::unique_ptr<Mesh, MeshDeleter> createSyntheticWaveMesh() {
        std::unique_ptr<Mesh, MeshDeleter> mesh(new Mesh("SyntheticWave"));

        struct CubeData {
            float lowPhase, highPhase;
            float lowAmp, highAmp;
            float sharpness;
        };

        const CubeData cubes[] = {
            { 0.03f, 0.05f, 0.22f, 0.28f, 0.20f },
            { 0.31f, 0.35f, 0.68f, 0.62f, 1.00f },
            { 0.57f, 0.61f, 0.18f, 0.24f, 0.45f },
            { 0.93f, 0.97f, 0.81f, 0.76f, 0.25f },
        };

        for (const auto& cubeData : cubes) {
            auto* cube = new VertCube(mesh.get());
            setCubeAsConstantPoint(cube,
                                   cubeData.lowPhase,
                                   cubeData.highPhase,
                                   cubeData.lowAmp,
                                   cubeData.highAmp,
                                   cubeData.sharpness);
            mesh->addCube(cube);
        }

        return mesh;
    }

    void requireFinite(Buffer<float> buffer) {
        for (int i = 0; i < buffer.size(); ++i) {
            INFO("index=" << i << " value=" << buffer[i]);
            REQUIRE(std::isfinite(buffer[i]));
        }
    }

    void requireNonDecreasing(Buffer<float> buffer, float tolerance = 2e-3f) {
        for (int i = 1; i < buffer.size(); ++i) {
            INFO("index=" << i
                 << " previous=" << buffer[i - 1]
                 << " current=" << buffer[i]
                 << " tolerance=" << tolerance);
            REQUIRE(buffer[i] + tolerance >= buffer[i - 1]);
        }
    }

    void configureWaveRasterizer(MeshRasterizer& rasterizer, Mesh* mesh) {
        rasterizer.setMesh(mesh);
        rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
        rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue));
    }
}

TEST_CASE("MeshRasterizer creates finite monotonic wave buffers for a synthetic mesh", "[meshrasterizer][wave]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    auto waveX = rasterizer.getWaveX();
    auto waveY = rasterizer.getWaveY();

    REQUIRE_FALSE(waveX.empty());
    REQUIRE(waveX.size() == waveY.size());
    REQUIRE(rasterizer.isSampleable());

    requireFinite(waveX);
    requireFinite(waveY);
    requireNonDecreasing(waveX);

    REQUIRE(waveX.front() < 0.f);
    REQUIRE(waveX.back() > 1.f);
    REQUIRE(rasterizer.getZeroIndex() >= 0);
    REQUIRE(rasterizer.getZeroIndex() < waveX.size() - 1);
    REQUIRE(rasterizer.getOneIndex() >= 0);
    REQUIRE(rasterizer.getOneIndex() < waveX.size() - 1);

}

TEST_CASE("MeshRasterizer can sample evenly after max-sharpness waveform generation", "[meshrasterizer][wave][sampling]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    ScopedAlloc<Float32> sampleMemory;
    sampleMemory.ensureSize(128);
    Buffer<float> samples = sampleMemory.place(128);
    rasterizer.sampleEvenlyTo(samples);

    requireFinite(samples);

}

TEST_CASE("WaveformSampler matches MeshRasterizer sampling adapters", "[meshrasterizer][sampling]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticWaveformSampler");
    configureWaveRasterizer(rasterizer, mesh.get());
    rasterizer.calcCrossPoints();

    Rasterization::WaveformBuffers waveform(
            rasterizer.getWaveX(),
            rasterizer.getWaveY(),
            rasterizer.getDiffX(),
            rasterizer.getSlopes(),
            Buffer<float>(),
            rasterizer.getZeroIndex(),
            rasterizer.getOneIndex());

    REQUIRE(Rasterization::WaveformSampler::isSampleable(waveform));
    REQUIRE(Rasterization::WaveformSampler::isSampleableAt(waveform, 0.5f));
    REQUIRE(Rasterization::WaveformSampler::sampleAt(waveform, false, 0.5) == rasterizer.sampleAt(0.5));

    int directIndex = rasterizer.getZeroIndex();
    int rasterizerIndex = rasterizer.getZeroIndex();
    REQUIRE(Rasterization::WaveformSampler::sampleAt(waveform, false, 0.5, directIndex)
            == rasterizer.sampleAt(0.5, rasterizerIndex));
    REQUIRE(directIndex == rasterizerIndex);

    ScopedAlloc<Float32> memory;
    memory.ensureSize(256);
    Buffer<float> intervals = memory.place(64);
    Buffer<float> direct = memory.place(64);
    Buffer<float> adapter = memory.place(64);

    for (int i = 0; i < intervals.size(); ++i) {
        intervals[i] = float(i) / float(intervals.size() - 1);
    }

    Rasterization::WaveformSampler::sampleAtIntervals(waveform, intervals, direct);
    rasterizer.sampleAtIntervals(intervals, adapter);
    RasterizerCompare::requireBufferNear(RasterizerCompare::copyBuffer(direct), RasterizerCompare::copyBuffer(adapter));

    Buffer<float> directEven = memory.place(32);
    Buffer<float> adapterEven = memory.place(32);
    float delta = 1.f / float(directEven.size() - 1);

    auto directPhase = Rasterization::WaveformSampler::sampleWithInterval(waveform, directEven, delta, 0.f);
    auto adapterPhase = rasterizer.sampleWithInterval(adapterEven, delta, 0.f);

    REQUIRE(directPhase == adapterPhase);
    RasterizerCompare::requireBufferNear(
            RasterizerCompare::copyBuffer(directEven),
            RasterizerCompare::copyBuffer(adapterEven));
}

TEST_CASE("GuideCurveSampler adds decoupled guide regions to waveform sampling", "[meshrasterizer][sampling][guide]") {
    ScopedAlloc<Float32> memory;
    memory.ensureSize(5);
    Buffer<float> waveX = memory.place(2);
    Buffer<float> waveY = memory.place(2);

    waveX[0] = 0.f;
    waveX[1] = 1.f;
    waveY[0] = 0.25f;
    waveY[1] = 0.25f;

    Buffer<float> diffX;
    Buffer<float> slope;
    Buffer<float> area;

    diffX.nullify();
    slope = memory.place(1);
    area.nullify();
    slope[0] = 0.f;

    Rasterization::WaveformBuffers waveform(waveX, waveY, diffX, slope, area, 0, 1);

    ConstantGuideCurveProvider provider;
    provider.values[3] = 0.5f;

    Rasterization::GuideCurveRegion region;
    region.guideIndex = 3;
    region.amplitude = 0.2f;
    region.start.x = 0.25f;
    region.end.x = 0.75f;

    std::vector<Rasterization::GuideCurveRegion> regions;
    regions.emplace_back(region);

    Rasterization::GuideCurveContext context;

    REQUIRE(Rasterization::GuideCurveSampler::sampleDecoupled(
            waveform,
            false,
            0.5,
            context,
            regions,
            &provider,
            7) == Catch::Approx(0.35f));

    REQUIRE(Rasterization::GuideCurveSampler::sampleDecoupled(
            waveform,
            false,
            0.9,
            context,
            regions,
            &provider,
            7) == Catch::Approx(0.25f));
}

TEST_CASE("MeshRasterizer characterization snapshot is deterministic", "[meshrasterizer][characterization]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer first("SyntheticMeshRasterizerA");
    MeshRasterizer second("SyntheticMeshRasterizerB");
    configureWaveRasterizer(first, mesh.get());
    configureWaveRasterizer(second, mesh.get());

    first.calcCrossPoints();
    first.makeCopy();
    second.calcCrossPoints();
    second.makeCopy();

    RasterizerCompare::requireSnapshotNear(
            RasterizerCompare::capture(first),
            RasterizerCompare::capture(second));

    ScopedAlloc<Float32> firstMemory;
    ScopedAlloc<Float32> secondMemory;
    firstMemory.ensureSize(128);
    secondMemory.ensureSize(128);

    Buffer<float> firstSamples = firstMemory.place(128);
    Buffer<float> secondSamples = secondMemory.place(128);
    first.sampleEvenlyTo(firstSamples);
    second.sampleEvenlyTo(secondSamples);

    RasterizerCompare::requireBufferNear(
            RasterizerCompare::copyBuffer(firstSamples),
            RasterizerCompare::copyBuffer(secondSamples));
}

TEST_CASE("MeshRasterizer characterizes hidden-dimension color points", "[meshrasterizer][characterization][colorpoints]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    configureWaveRasterizer(rasterizer, mesh.get());
    rasterizer.calcCrossPoints();
    rasterizer.makeCopy();

    const auto& colorPoints = rasterizer.getColorPoints();
    REQUIRE(colorPoints.size() == mesh->getNumCubes() * 3);

    int timePoints = 0;
    int redPoints = 0;
    int bluePoints = 0;

    for (const auto& point : colorPoints) {
        INFO("hiddenDim=" << point.num);
        REQUIRE(point.cube != nullptr);
        REQUIRE(std::isfinite(point.before.x));
        REQUIRE(std::isfinite(point.before.y));
        REQUIRE(std::isfinite(point.mid.x));
        REQUIRE(std::isfinite(point.mid.y));
        REQUIRE(std::isfinite(point.after.x));
        REQUIRE(std::isfinite(point.after.y));

        if (point.num == Vertex::Time) {
            ++timePoints;
        } else if (point.num == Vertex::Red) {
            ++redPoints;
        } else if (point.num == Vertex::Blue) {
            ++bluePoints;
        } else {
            FAIL("unexpected hidden dimension");
        }
    }

    REQUIRE(timePoints == mesh->getNumCubes());
    REQUIRE(redPoints == mesh->getNumCubes());
    REQUIRE(bluePoints == mesh->getNumCubes());
    REQUIRE(rasterizer.getRastData().colorPoints.size() == colorPoints.size());
}

TEST_CASE("MeshSlicePipeline matches MeshRasterizer intercept and color-point slicing", "[meshrasterizer][pipeline][slice]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    configureWaveRasterizer(rasterizer, mesh.get());
    rasterizer.calcCrossPoints();
    rasterizer.makeCopy();

    Rasterization::MeshSlicePipeline pipeline;
    Rasterization::TrilinearMeshSlicer slicer;
    Rasterization::MeshCubeSource source(mesh.get());
    Rasterization::RasterizationRequest request = rasterizer.createRasterizationRequest();
    const auto& output = pipeline.render(
            source,
            slicer,
            request,
            0.f,
            [](Intercept&, const MorphPosition&, bool) {});

    REQUIRE(output.sampleable);
    REQUIRE(output.intercepts.size() == rasterizer.getRastData().intercepts.size());

    for (int i = 0; i < (int) output.intercepts.size(); ++i) {
        INFO("intercept=" << i);
        RasterizerCompare::requireInterceptNear(output.intercepts[i], rasterizer.getRastData().intercepts[i]);
    }

    REQUIRE(output.colorPoints.size() == rasterizer.getRastData().colorPoints.size());

    for (int i = 0; i < (int) output.colorPoints.size(); ++i) {
        INFO("colorPoint=" << i);
        RasterizerCompare::requireColorPointNear(output.colorPoints[i], rasterizer.getRastData().colorPoints[i]);
    }
}

TEST_CASE("InterceptSortPolicy sorts only when requested", "[meshrasterizer][pipeline][intercepts]") {
    vector<Intercept> intercepts {
        Intercept(0.75f, 0.1f),
        Intercept(0.25f, 0.2f),
    };

    bool needsResorting = false;
    Rasterization::InterceptSortPolicy(&needsResorting).sortIfNeeded(intercepts);

    REQUIRE(intercepts[0].x == Catch::Approx(0.75f));
    REQUIRE(intercepts[1].x == Catch::Approx(0.25f));
    REQUIRE_FALSE(needsResorting);

    needsResorting = true;
    Rasterization::InterceptSortPolicy(&needsResorting).sortIfNeeded(intercepts);

    REQUIRE(intercepts[0].x == Catch::Approx(0.25f));
    REQUIRE(intercepts[1].x == Catch::Approx(0.75f));
    REQUIRE_FALSE(needsResorting);
}

TEST_CASE("InterceptDegeneracyPolicy classifies empty and single intercept outputs", "[meshrasterizer][pipeline][intercepts]") {
    Rasterization::InterceptDegeneracyPolicy policy;

    REQUIRE(policy.classify(0) == Rasterization::InterceptDegeneracyAction::CleanUp);
    REQUIRE(policy.classify(1) == Rasterization::InterceptDegeneracyAction::MarkUnsampleable);
    REQUIRE(policy.classify(2) == Rasterization::InterceptDegeneracyAction::Continue);
    REQUIRE(policy.classify(3) == Rasterization::InterceptDegeneracyAction::Continue);
}

TEST_CASE("InterceptPaddingFlagPolicy marks consolidated component-guide padding spans", "[meshrasterizer][pipeline][intercepts]") {
    auto mesh = createSyntheticWaveMesh();
    VertCube* componentCube = mesh->getCubes().front();
    VertCube* plainCube = mesh->getCubes().back();
    componentCube->getCompGuideCurve() = 0;

    vector<Intercept> intercepts {
        Intercept(0.1f, 0.1f, componentCube),
        Intercept(0.2f, 0.2f, componentCube),
        Intercept(0.3f, 0.3f, plainCube),
        Intercept(0.4f, 0.4f, plainCube),
    };

    Rasterization::InterceptPaddingFlagPolicy().apply(intercepts);

    REQUIRE(intercepts[0].padBefore);
    REQUIRE_FALSE(intercepts[1].padBefore);
    REQUIRE_FALSE(intercepts[2].padBefore);
    REQUIRE_FALSE(intercepts[3].padBefore);
    REQUIRE_FALSE(intercepts[0].padAfter);
    REQUIRE_FALSE(intercepts[1].padAfter);
    REQUIRE(intercepts[2].padAfter);
    REQUIRE_FALSE(intercepts[3].padAfter);

    vector<Intercept> plainIntercepts {
        Intercept(0.1f, 0.1f, plainCube),
        Intercept(0.2f, 0.2f, plainCube),
        Intercept(0.3f, 0.3f, plainCube),
    };

    Rasterization::InterceptPaddingFlagPolicy().apply(plainIntercepts);

    for (const Intercept& intercept : plainIntercepts) {
        REQUIRE_FALSE(intercept.padBefore);
        REQUIRE_FALSE(intercept.padAfter);
    }
}

TEST_CASE("GuideCurvePolicy applies phase wrapping and amplitude clamping", "[meshrasterizer][pipeline][guide]") {
    auto mesh = createSyntheticWaveMesh();
    VertCube* cube = mesh->getCubes().front();
    cube->guideCurveAt(Vertex::Phase) = 0;
    cube->guideCurveAt(Vertex::Amp) = 1;

    ConstantGuideCurveProvider provider;
    provider.values[0] = 0.10f;
    provider.values[1] = 2.00f;

    VertCube::ReductionData reduction;
    MorphPosition position(0.5f, 0.5f, 0.5f);
    Rasterization::TrilinearMeshSlicer().slice(*cube, Vertex::Time, reduction, position);
    VertCube::vertexAt(position.time, Vertex::Time, &reduction.v0, &reduction.v1, &reduction.v);

    bool needsResorting = false;
    Intercept intercept(0.95f, 0.9f, cube, 0.5f);
    intercept.adjustedX = intercept.x;

    Rasterization::GuideCurvePolicyContext context;
    context.guideCurveProvider = &provider;
    context.reduction = &reduction;
    context.scalingMode = Rasterization::PointScalingMode::Bipolar;
    context.cyclic = true;
    context.needsResorting = &needsResorting;

    Rasterization::GuideCurvePolicy(context).apply(intercept, position);

    REQUIRE(intercept.adjustedX == Catch::Approx(0.05f).margin(1e-6f));
    REQUIRE(intercept.isWrapped);
    REQUIRE(needsResorting);
    REQUIRE(intercept.y == Catch::Approx(1.f));
}

TEST_CASE("CurveWaveformPreparationPolicy prepares curves before waveform baking", "[meshrasterizer][pipeline][waveform]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();
    VertCube* componentCube = mesh->getCubes().front();
    VertCube* plainCube = mesh->getCubes().back();
    componentCube->getCompGuideCurve() = 0;

    vector<Curve> curves {
        Curve(
                Intercept(0.0f, 0.0f),
                Intercept(0.1f, 0.1f, componentCube, 0.2f),
                Intercept(0.2f, 0.2f, componentCube, 0.3f)),
        Curve(
                Intercept(0.2f, 0.2f, componentCube, 0.4f),
                Intercept(0.3f, 0.3f, plainCube, 0.5f),
                Intercept(0.4f, 0.4f, plainCube, 0.6f)),
    };

    Rasterization::CurveWaveformPreparationPolicy().apply(curves);

    REQUIRE(curves[0].c.shp == 1.f);
    REQUIRE(curves[1].b.shp == 1.f);
    REQUIRE(std::isfinite(curves[1].transformX[0]));
    REQUIRE(std::isfinite(curves[1].transformY[0]));
}

TEST_CASE("MeshWaveformRasterizer preserves component guide waveform baking", "[meshrasterizer][pipeline][composer][guide]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();
    mesh->getCubes().front()->getCompGuideCurve() = 0;

    ConstantGuideCurveProvider provider;
    provider.values[0] = 0.25f;

    MeshRasterizer reference("ComponentGuideReference");
    configureWaveRasterizer(reference, mesh.get());
    reference.setGuideCurveProvider(&provider);
    reference.calcCrossPoints();

    Rasterization::MeshWaveformRasterizer composed;
    composed.getRequest() = reference.createRasterizationRequest();
    composed.setGuideCurveProvider(&provider);
    composed.render(mesh.get());

    REQUIRE(reference.isSampleable());
    REQUIRE(composed.isSampleable());
    REQUIRE(composed.waveform().waveX.size() == reference.getWaveX().size());

    RasterizerCompare::requireBufferNear(
            RasterizerCompare::copyBuffer(reference.getWaveX()),
            RasterizerCompare::copyBuffer(composed.waveform().waveX));
    RasterizerCompare::requireBufferNear(
            RasterizerCompare::copyBuffer(reference.getWaveY()),
            RasterizerCompare::copyBuffer(composed.waveform().waveY));
}

TEST_CASE("MeshRasterizer characterizes intercept restriction spacing", "[meshrasterizer][characterization][restriction]") {
    MeshRasterizer rasterizer("RestrictionRasterizer");

    vector<Intercept> intercepts {
        Intercept(0.2f, 0.1f),
        Intercept(0.2f, 0.2f),
        Intercept(0.1f, 0.3f),
        Intercept(1.2f, 0.4f),
    };

    for (auto& intercept : intercepts) {
        intercept.adjustedX = intercept.x;
    }

    rasterizer.restrictIntercepts(intercepts);

    REQUIRE(intercepts.front().x >= 0.f);
    REQUIRE(intercepts.back().x <= 1.f);

    for (int i = 1; i < (int) intercepts.size(); ++i) {
        INFO("index=" << i << " left=" << intercepts[i - 1].x << " right=" << intercepts[i].x);
        REQUIRE(intercepts[i].x > intercepts[i - 1].x);
        REQUIRE(intercepts[i].x - intercepts[i - 1].x >= Catch::Approx(0.0001f).margin(1e-6f));
    }
}
