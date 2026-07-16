#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include <Curve/Curve.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

namespace {
    struct CurveTableScope {
        CurveTableScope() : ownsTable(Curve::table == nullptr) {
            if (ownsTable) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (ownsTable) {
                Curve::deleteTable();
            }
        }

        bool ownsTable;
    };

    struct TestEnvelope {
        TestEnvelope() : mesh("PreparedEnvelope") {
            constexpr float phases[] { 0.0f, 0.45f, 1.0f };
            constexpr float amplitudes[] { 0.0f, 0.8f, 0.3f };
            for (int cubeIndex = 0; cubeIndex < 3; ++cubeIndex) {
                auto* cube = new VertCube(&mesh);
                for (int vertexIndex = 0; vertexIndex < (int) VertCube::numVerts; ++vertexIndex) {
                    bool time, red, blue;
                    VertCube::getPoles(vertexIndex, time, red, blue);
                    Vertex* vertex = cube->getVertex(vertexIndex);
                    vertex->values[Vertex::Time] = time ? 1.f : 0.f;
                    vertex->values[Vertex::Red] = red ? 1.f : 0.f;
                    vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
                    vertex->values[Vertex::Phase] = phases[cubeIndex];
                    vertex->values[Vertex::Amp] = amplitudes[cubeIndex];
                    vertex->values[Vertex::Curve] = 0.5f;
                }
                mesh.addCube(cube);
            }
            mesh.loopCubes.insert(mesh.getCubes()[0]);
            mesh.sustainCubes.insert(mesh.getCubes()[1]);
        }

        ~TestEnvelope() {
            mesh.destroy();
        }

        EnvelopeMesh mesh;
    };
}

TEST_CASE("Envelope preparation is independent of snapshot publication", "[rasterization][env][boundary]") {
    CurveTableScope curveTable;
    TestEnvelope envelope;
    EnvRasterizer rasterizer;

    rasterizer.renderWaveformOnly(&envelope.mesh);
    REQUIRE(rasterizer.sampler().isSampleable());
    REQUIRE(rasterizer.preparedResult().intercepts.size() >= 3);
    {
        auto unpublished = rasterizer.snapshotView();
        REQUIRE(unpublished.intercepts().empty());
        REQUIRE(unpublished.curves().empty());
        REQUIRE_FALSE(unpublished.isSampleable());
    }

    rasterizer.publishCurrentResult();
    {
        auto published = rasterizer.snapshotView();
        REQUIRE(published.intercepts().size() == rasterizer.preparedResult().intercepts.size());
        REQUIRE(published.curves().size() == rasterizer.preparedResult().curves.size());
        REQUIRE(published.isSampleable());
    }
}

TEST_CASE("Envelope cleanup publishes one complete empty generation", "[rasterization][env][boundary]") {
    CurveTableScope curveTable;
    TestEnvelope envelope;
    EnvRasterizer rasterizer;
    rasterizer.updateWaveform(&envelope.mesh);

    REQUIRE(rasterizer.snapshotView().isSampleable());
    rasterizer.cleanUp();

    auto empty = rasterizer.snapshotView();
    REQUIRE(empty.intercepts().empty());
    REQUIRE(empty.curves().empty());
    REQUIRE(empty.waveX().empty());
    REQUIRE(empty.waveY().empty());
    REQUIRE_FALSE(empty.isSampleable());
}

TEST_CASE("Adopting prepared envelope data does not publish", "[rasterization][env][boundary]") {
    CurveTableScope curveTable;
    TestEnvelope envelope;
    EnvRasterizer source;
    source.renderWaveformOnly(&envelope.mesh);

    EnvRasterizer destination;
    destination.adoptPreparedData(source);
    REQUIRE(destination.sampler().isSampleable());
    REQUIRE(destination.sampler().sampleAt(0.4) == Catch::Approx(source.sampler().sampleAt(0.4)));
    {
        auto unpublished = destination.snapshotView();
        REQUIRE(unpublished.intercepts().empty());
        REQUIRE_FALSE(unpublished.isSampleable());
    }

    destination.publishCurrentResult();
    auto published = destination.snapshotView();
    REQUIRE(published.intercepts().size() == source.preparedResult().intercepts.size());
    REQUIRE(published.isSampleable());
}

TEST_CASE("Envelope playback states advance lifecycle independently", "[rasterization][env][playback]") {
    Rasterization::EnvelopePlaybackState first;
    Rasterization::EnvelopePlaybackState second;
    first.ensureVoiceCount(4);
    second.ensureVoiceCount(4);

    first.voice(2).samplePosition = 0.75;
    second.voice(2).samplePosition = 0.25;
    first.requestRelease(true);

    REQUIRE(first.mode == Rasterization::EnvelopePlaybackMode::Releasing);
    REQUIRE(first.consumeReleaseRequest());
    REQUIRE_FALSE(first.consumeReleaseRequest());
    REQUIRE(second.mode == Rasterization::EnvelopePlaybackMode::Normal);
    REQUIRE(second.voice(2).samplePosition == Catch::Approx(0.25));

    first.noteOn(1);
    REQUIRE(first.mode == Rasterization::EnvelopePlaybackMode::Normal);
    REQUIRE(first.voice(2).samplePosition == Catch::Approx(0.0));
    REQUIRE(second.voice(2).samplePosition == Catch::Approx(0.25));
}

TEST_CASE("Envelope playback does not mutate prepared display data", "[rasterization][env][playback]") {
    CurveTableScope curveTable;
    TestEnvelope envelope;
    EnvRasterizer rasterizer;
    rasterizer.renderWaveformOnly(&envelope.mesh);
    const auto& prepared = rasterizer.preparedResult();
    const auto originalIntercepts = prepared.intercepts;
    const auto originalCurves = prepared.curves;
    std::vector<float> originalWaveY(
            prepared.waveform.waveY.get(),
            prepared.waveform.waveY.get() + prepared.waveform.waveY.size());

    MeshLibrary::EnvProps props;
    props.active = true;
    rasterizer.setNoteOn();
    REQUIRE(rasterizer.renderToBuffer(12, 0.1, EnvRasterizer::headUnisonIndex, props, 1.f));
    REQUIRE(rasterizer.getMode() == EnvRasterizer::Looping);
    REQUIRE(prepared.intercepts == originalIntercepts);
    REQUIRE(prepared.curves == originalCurves);
    REQUIRE(std::equal(
            originalWaveY.begin(),
            originalWaveY.end(),
            prepared.waveform.waveY.get()));

    rasterizer.setNoteOff();
    rasterizer.renderToBuffer(4, 0.1, EnvRasterizer::headUnisonIndex, props, 1.f);
    REQUIRE(rasterizer.getMode() == EnvRasterizer::Releasing);
    REQUIRE(prepared.intercepts == originalIntercepts);
    REQUIRE(prepared.curves == originalCurves);
    REQUIRE(std::equal(
            originalWaveY.begin(),
            originalWaveY.end(),
            prepared.waveform.waveY.get()));
}
