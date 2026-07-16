#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Curve.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>

namespace {
    using Catch::Approx;

    struct CurveTableScope {
        CurveTableScope() {
            Curve::calcTable();
        }

        ~CurveTableScope() {
            Curve::deleteTable();
        }
    };

    void setCubeAsVoicePoint(
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
            vertex->values[Vertex::Time] = time ? 1.f : 0.f;
            vertex->values[Vertex::Red] = red ? 1.f : 0.f;
            vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp] = time ? highAmp : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    void addVoiceCube(
            Mesh& mesh,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        auto* cube = new VertCube(&mesh);
        setCubeAsVoicePoint(cube, lowPhase, highPhase, lowAmp, highAmp, sharpness);
        mesh.addCube(cube);
    }
}

TEST_CASE("Shared voice rasterizer builds chained slice intercepts", "[rasterization][voice]") {
    CurveTableScope curveTable;
    Mesh mesh("VoiceSliceMesh");
    addVoiceCube(mesh, 0.20f, 0.80f, 0.25f, 0.75f, 0.35f);
    addVoiceCube(mesh, 0.10f, 0.40f, 0.10f, 0.30f, 0.55f);

    Rasterization::VoiceCycleState state;
    Rasterization::VoiceRasterizer rasterizer;
    rasterizer.setMesh(&mesh);
    rasterizer.setState(&state);
    rasterizer.setMorphPosition(MorphPosition(0.50f, 0.50f, 0.50f));

    rasterizer.renderChained(0.85f);
    rasterizer.renderChained(0.85f);
    rasterizer.publishCurrentResult();

    auto snapshot = rasterizer.snapshotView();
    const auto& intercepts = snapshot.intercepts();
    REQUIRE(intercepts.size() == 2);
    REQUIRE(intercepts[0].x == Approx(0.10f));
    REQUIRE(intercepts[0].y == Approx(-0.60f));
    REQUIRE(intercepts[0].shp == Approx(0.55f));
    REQUIRE(intercepts[1].x == Approx(0.35f));
    REQUIRE(intercepts[1].y == Approx(0.f));
    REQUIRE(intercepts[1].shp == Approx(0.35f));

    mesh.destroy();
}

TEST_CASE("Voice rendering selects each result without publishing", "[rasterization][voice]") {
    CurveTableScope curveTable;
    Mesh mesh("VoiceRenderBoundaryMesh");
    addVoiceCube(mesh, 0.20f, 0.80f, 0.25f, 0.75f, 0.35f);
    addVoiceCube(mesh, 0.10f, 0.40f, 0.10f, 0.30f, 0.55f);

    Rasterization::VoiceCycleState state;
    Rasterization::VoiceRasterizer rasterizer;
    rasterizer.setMesh(&mesh);
    rasterizer.setState(&state);
    rasterizer.setMorphPosition(MorphPosition(0.50f, 0.50f, 0.50f));

    rasterizer.renderChained(0.85f);
    const auto& chained = rasterizer.renderChained(0.85f);
    REQUIRE(chained.sampleable);
    REQUIRE(rasterizer.sampler().sampleAt(0.25)
            == Approx(Rasterization::VoiceRasterizer::sampler(chained).sampleAt(0.25)));
    {
        auto unpublished = rasterizer.snapshotView();
        REQUIRE(unpublished.intercepts().empty());
        REQUIRE_FALSE(unpublished.isSampleable());
    }

    const auto& ordinary = rasterizer.renderOrdinary(&mesh, 0.15f);
    REQUIRE(ordinary.sampleable);
    REQUIRE(rasterizer.sampler().sampleAt(0.25)
            == Approx(Rasterization::VoiceRasterizer::sampler(ordinary).sampleAt(0.25)));
    {
        auto stillUnpublished = rasterizer.snapshotView();
        REQUIRE(stillUnpublished.intercepts().empty());
    }

    rasterizer.publishCurrentResult();
    {
        auto published = rasterizer.snapshotView();
        REQUIRE(published.isSampleable() == ordinary.sampleable);
        REQUIRE(published.intercepts().size() == ordinary.intercepts.size());
    }

    const auto& chainedAgain = rasterizer.renderChained(0.45f);
    REQUIRE(rasterizer.sampler().sampleAt(0.25)
            == Approx(Rasterization::VoiceRasterizer::sampler(chainedAgain).sampleAt(0.25)));

    mesh.destroy();
}

TEST_CASE("Prepared voice renders reuse result storage", "[rasterization][voice]") {
    CurveTableScope curveTable;
    Mesh mesh("VoiceRenderStorageMesh");
    addVoiceCube(mesh, 0.20f, 0.80f, 0.25f, 0.75f, 0.35f);
    addVoiceCube(mesh, 0.10f, 0.40f, 0.10f, 0.30f, 0.55f);

    Rasterization::VoiceCycleState state;
    Rasterization::VoiceRasterizer rasterizer;
    rasterizer.setMesh(&mesh);
    rasterizer.setState(&state);
    rasterizer.setMorphPosition(MorphPosition(0.50f, 0.50f, 0.50f));

    const std::initializer_list<float> phases { 0.20f, 0.35f, 0.55f, 0.75f };
    for (float phase : phases) {
        rasterizer.renderOrdinary(&mesh, phase);
    }

    const auto& ordinary = rasterizer.renderOrdinary(&mesh, 0.10f);
    const auto* ordinaryInterceptStorage = ordinary.intercepts.data();
    const auto* ordinaryCurveStorage = ordinary.curves.data();
    const auto* ordinaryWaveformStorage = ordinary.waveform.waveX.get();
    for (float phase : phases) {
        const auto& next = rasterizer.renderOrdinary(&mesh, phase);
        REQUIRE(next.intercepts.data() == ordinaryInterceptStorage);
        REQUIRE(next.curves.data() == ordinaryCurveStorage);
        REQUIRE(next.waveform.waveX.get() == ordinaryWaveformStorage);
    }

    rasterizer.renderChained(0.10f);
    rasterizer.renderChained(0.20f);
    rasterizer.renderChained(0.30f);
    for (float phase : phases) {
        rasterizer.renderChained(phase);
    }

    const auto& chained = rasterizer.renderChained(0.30f);
    const auto* chainedInterceptStorageA = chained.intercepts.data();
    const Intercept* chainedInterceptStorageB = nullptr;
    const auto* chainedCurveStorage = chained.curves.data();
    const auto* chainedWaveformStorage = chained.waveform.waveX.get();
    for (float phase : phases) {
        const auto& next = rasterizer.renderChained(phase);
        if (next.intercepts.data() != chainedInterceptStorageA && chainedInterceptStorageB == nullptr) {
            chainedInterceptStorageB = next.intercepts.data();
        }
        REQUIRE((next.intercepts.data() == chainedInterceptStorageA
                || next.intercepts.data() == chainedInterceptStorageB));
        REQUIRE(next.curves.data() == chainedCurveStorage);
        REQUIRE(next.waveform.waveX.get() == chainedWaveformStorage);
    }
    REQUIRE(chainedInterceptStorageB != nullptr);

    mesh.destroy();
}
