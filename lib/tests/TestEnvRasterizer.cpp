#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include <Curve/Curve.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/EnvelopeMaterialization.h>
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

    class ConstantGuideCurveProvider : public GuideCurveProvider {
    public:
        float getTableValue(
                int guideIndex,
                float,
                const NoiseContext&) override {
            return values[guideIndex];
        }

        void sampleDownAddNoise(
                int guideIndex,
                Buffer<float> destination,
                const NoiseContext&) override {
            destination.set(values[guideIndex]);
        }

        Buffer<Float32> getTable(int) override { return {}; }
        int getTableDensity(int) override { return tableSize; }

        float values[128] {};
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
                    scalingMode,
                    nullptr,
                    -1
            };
        }

        Rasterization::RenderResult display;
        Rasterization::RenderResult loop;
        Rasterization::PointScalingMode scalingMode { Rasterization::PointScalingMode::Unipolar };
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

TEST_CASE("Realtime Envelope materialization is byte-identical to EnvRasterizer",
        "[rasterization][env][realtime][parity]") {
    CurveTableScope curveTable;
    TestEnvelope envelope;
    ConstantGuideCurveProvider guideCurveProvider;
    guideCurveProvider.values[0] = 0.125f;
    envelope.mesh.getCubes()[1]->getCompGuideCurve() = 0;
    const MorphPosition morph(0.f, 0.2f, 0.8f);
    EnvRasterizer rasterizer;
    rasterizer.setGuideCurveProvider(&guideCurveProvider);
    rasterizer.setMorphPosition(morph);
    rasterizer.renderWaveformOnly(&envelope.mesh);

    Rasterization::RealtimeEnvelopePlan plan;
    plan.mesh = &envelope.mesh;
    plan.guideCurveProvider = &guideCurveProvider;
    plan.request.morph = morph;
    plan.request.cyclic = false;
    plan.request.xMinimum = 0.f;
    plan.request.xMaximum = 10.f;
    plan.capacity = Rasterization::envelopeMaterializationCapacity(
            envelope.mesh,
            plan.request,
            plan.guideCurveProvider);
    Rasterization::RealtimeEnvelopeMaterializer materializer;
    REQUIRE(materializer.prepare(plan));
    REQUIRE(materializer.materialize(morph.red, morph.blue));

    const auto realtime = materializer.preparedPlaybackView();
    const auto legacy = rasterizer.preparedPlaybackView();
    REQUIRE(realtime.loopIndex == legacy.loopIndex);
    REQUIRE(realtime.sustainIndex == legacy.sustainIndex);
    REQUIRE(realtime.display.intercepts == legacy.display.intercepts);
    REQUIRE(realtime.display.curves == legacy.display.curves);
    REQUIRE(realtime.display.waveform.waveX.size() == legacy.display.waveform.waveX.size());
    REQUIRE(std::equal(
            realtime.display.waveform.waveX.get(),
            realtime.display.waveform.waveX.get() + realtime.display.waveform.waveX.size(),
            legacy.display.waveform.waveX.get()));
    REQUIRE(std::equal(
            realtime.display.waveform.waveY.get(),
            realtime.display.waveform.waveY.get() + realtime.display.waveform.waveY.size(),
            legacy.display.waveform.waveY.get()));
    REQUIRE(realtime.loop.intercepts == legacy.loop.intercepts);
    REQUIRE(realtime.loop.curves == legacy.loop.curves);
    REQUIRE(std::equal(
            realtime.loop.waveform.waveY.get(),
            realtime.loop.waveform.waveY.get() + realtime.loop.waveform.waveY.size(),
            legacy.loop.waveform.waveY.get()));

    const auto diagnostics = materializer.diagnostics();
    REQUIRE(diagnostics.attempts == 1);
    REQUIRE(diagnostics.failures == 0);
    REQUIRE(diagnostics.maximumElapsedNanoseconds > 0);
    REQUIRE(diagnostics.highWater.intercepts <= plan.capacity.intercepts);
    REQUIRE(diagnostics.highWater.curves <= plan.capacity.curves);
    REQUIRE(diagnostics.highWater.waveformSamples <= plan.capacity.waveformSamples);
}

TEST_CASE("Realtime Envelope plans reject unsupported capacity before preparation",
        "[rasterization][env][realtime][capacity]") {
    EnvelopeMesh mesh("OversizedEnvelope");
    Rasterization::RealtimeEnvelopePlan plan;
    plan.mesh = &mesh;
    plan.capacity.intercepts = Rasterization::EnvelopeMaterializationCapacity::maximumIntercepts + 1;
    plan.capacity.curves = plan.capacity.intercepts + 6;
    plan.capacity.guideCurveRegions = plan.capacity.curves;
    plan.capacity.waveformSamples = 32;

    Rasterization::RealtimeEnvelopeMaterializer materializer;
    REQUIRE_FALSE(materializer.prepare(plan));
    REQUIRE(materializer.diagnostics().attempts == 0);
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

TEST_CASE(
        "Envelope release begins at the held bipolar level",
        "[rasterization][env][playback][release]") {
    TestPreparedPlayback prepared;
    prepared.scalingMode = Rasterization::PointScalingMode::Bipolar;

    const float amplitudes[] { 1.f, 0.75f, 0.25f, 0.f, 0.f };
    for (int i = 0; i < prepared.display.waveform.waveY.size(); ++i) {
        prepared.display.waveform.waveY[i] = amplitudes[i];
        prepared.display.waveform.slope[i] = i < 4
                ? (amplitudes[i + 1] - amplitudes[i]) / 0.5f
                : 0.f;
    }

    Rasterization::EnvelopePlaybackEngine playback;
    MeshLibrary::EnvProps props;
    props.active = true;
    playback.noteOn();

    REQUIRE(playback.renderToBuffer(prepared.view(), 1, 0.1, 1, props, 1.f));
    const float heldLevel = playback.output()[0];
    REQUIRE(heldLevel == Catch::Approx(0.75f));

    playback.noteOff(prepared.view());
    REQUIRE(playback.renderToBuffer(prepared.view(), 1, 0.1, 1, props, 1.f));
    REQUIRE(playback.output()[0] == Catch::Approx(heldLevel));
}

TEST_CASE(
        "Envelope release renders every sample before its terminal boundary",
        "[rasterization][env][playback][release]") {
    TestPreparedPlayback prepared;
    prepared.scalingMode = Rasterization::PointScalingMode::Bipolar;

    const float amplitudes[] { 1.f, 0.75f, 0.75f, 0.f, 0.f };
    for (int i = 0; i < prepared.display.waveform.waveY.size(); ++i) {
        prepared.display.waveform.waveY[i] = amplitudes[i];
        prepared.display.waveform.slope[i] = i < 4
                ? (amplitudes[i + 1] - amplitudes[i]) / 0.5f
                : 0.f;
    }

    Rasterization::EnvelopePlaybackEngine playback;
    MeshLibrary::EnvProps props;
    props.active = true;
    playback.noteOn();
    REQUIRE(playback.renderToBuffer(prepared.view(), 1, 0.3, 1, props, 1.f));

    playback.noteOff(prepared.view());
    REQUIRE(playback.releaseSamplesRemaining(prepared.view(), 0.3, 1, props, 1.f) == 2);
    REQUIRE_FALSE(playback.renderToBuffer(prepared.view(), 3, 0.3, 1, props, 1.f));
    REQUIRE(playback.output()[0] == Catch::Approx(0.75f));
    REQUIRE(playback.output()[1] == Catch::Approx(0.3f));
    REQUIRE(playback.output()[2] == 0.f);
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
