#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include <Curve/Curve.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/EnvelopePlaybackEngine.h>
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

    struct TestPreparedPlayback {
        TestPreparedPlayback() {
            display.intercepts = {
                Intercept(0.f, 0.f, nullptr, 0.5f),
                Intercept(0.5f, 0.5f, nullptr, 0.5f),
                Intercept(1.f, 1.f, nullptr, 0.5f)
            };
            display.waveform.place(display.waveformMemory, 5);
            for (int i = 0; i < 5; ++i) {
                display.waveform.waveX[i] = -0.5f + 0.5f * (float) i;
                display.waveform.waveY[i] = -0.5f + 0.5f * (float) i;
                display.waveform.diffX[i] = i < 4 ? 0.5f : 0.f;
                display.waveform.slope[i] = i < 4 ? 1.f : 0.f;
                display.waveform.area[i] = 0.f;
            }
            display.waveform.zeroIndex = 1;
            display.waveform.oneIndex = 3;
            display.sampleable = true;
        }

        Rasterization::PreparedEnvelopePlaybackView view() const {
            return {
                    display,
                    loop,
                    -1,
                    1,
                    Rasterization::PointScalingMode::Unipolar,
                    nullptr,
                    -1
            };
        }

        Rasterization::RenderResult display;
        Rasterization::RenderResult loop;
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

TEST_CASE("Envelope playback engines independently consume one prepared envelope", "[rasterization][env][playback]") {
    TestPreparedPlayback prepared;
    const auto originalIntercepts = prepared.display.intercepts;
    std::vector<float> originalWaveY(
            prepared.display.waveform.waveY.get(),
            prepared.display.waveform.waveY.get() + prepared.display.waveform.waveY.size());
    Rasterization::EnvelopePlaybackEngine first;
    Rasterization::EnvelopePlaybackEngine second;
    MeshLibrary::EnvProps props;
    props.active = true;

    first.noteOn();
    second.noteOn();
    REQUIRE(first.renderToBuffer(prepared.view(), 3, 0.1, 1, props, 1.f));
    REQUIRE(first.renderToBuffer(prepared.view(), 2, 0.1, 1, props, 1.f));
    REQUIRE(second.renderToBuffer(prepared.view(), 2, 0.1, 1, props, 1.f));

    REQUIRE(first.output()[0] > second.output()[0]);
    REQUIRE(first.sustainLevel(1) > second.sustainLevel(1));
    REQUIRE(prepared.display.intercepts == originalIntercepts);
    REQUIRE(std::equal(
            originalWaveY.begin(),
            originalWaveY.end(),
            prepared.display.waveform.waveY.get()));

    first.noteOff(prepared.view());
    REQUIRE(first.mode() == Rasterization::EnvelopePlaybackMode::Releasing);
    first.renderToBuffer(prepared.view(), 2, 0.1, 1, props, 1.f);
    REQUIRE(second.mode() == Rasterization::EnvelopePlaybackMode::Normal);
}

TEST_CASE("Prepared Envelope replacement preserves and reconciles live playback state",
        "[rasterization][env][playback][replacement]") {
    TestPreparedPlayback prepared;
    Rasterization::EnvelopePlaybackEngine playback;
    MeshLibrary::EnvProps props;
    props.active = true;
    playback.noteOn();

    REQUIRE(playback.renderToBuffer(prepared.view(), 3, 0.1, 1, props, 1.f));
    const double beforeReplacement = playback.samplePosition(1);
    playback.validate(prepared.view());
    REQUIRE(playback.samplePosition(1) == Catch::Approx(beforeReplacement));
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Normal);

    const Rasterization::PreparedEnvelopePlaybackView looping {
            prepared.display,
            prepared.loop,
            0,
            1,
            Rasterization::PointScalingMode::Unipolar,
            nullptr,
            -1
    };
    REQUIRE(playback.renderToBuffer(looping, 4, 0.1, 1, props, 1.f));
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Looping);
    playback.validate(prepared.view());
    REQUIRE(playback.mode() == Rasterization::EnvelopePlaybackMode::Normal);
    REQUIRE(playback.samplePosition(1) <= prepared.display.waveform.waveX.back());
}

TEST_CASE("Envelope playback voice offsets follow explicit lifecycle seeds",
        "[rasterization][env][seed]") {
    Rasterization::EnvelopePlaybackEngine first;
    Rasterization::EnvelopePlaybackEngine repeated;
    Rasterization::EnvelopePlaybackEngine secondVoice;
    first.ensureVoiceCount(2);
    repeated.ensureVoiceCount(2);
    secondVoice.ensureVoiceCount(2);

    first.deriveVoiceOffsets(
            256,
            Rasterization::GuideCurveSeed::voiceLifecycle(101u));
    repeated.deriveVoiceOffsets(
            256,
            Rasterization::GuideCurveSeed::voiceLifecycle(101u));
    secondVoice.deriveVoiceOffsets(
            256,
            Rasterization::GuideCurveSeed::voiceLifecycle(202u));

    REQUIRE(first.voiceOffsetSeeds(1) == repeated.voiceOffsetSeeds(1));
    REQUIRE(first.voiceOffsetSeeds(2) == repeated.voiceOffsetSeeds(2));
    REQUIRE(first.voiceOffsetSeeds(1) != secondVoice.voiceOffsetSeeds(1));
    REQUIRE(first.voiceOffsetSeeds(2) != secondVoice.voiceOffsetSeeds(2));
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
