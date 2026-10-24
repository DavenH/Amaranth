#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/VertCube.h>

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Runtime/PreviewTraversal.h"
#include "Runtime/TrimeshMorphResolver.h"
#include "UI/PanelPlaybackOverlay.h"

using namespace CycleV2;
using namespace juce;

namespace {

std::shared_ptr<const EnvelopeConfiguration> loopEnvelope() {
    EnvelopeNodeModel model;
    auto& mesh = model.getMesh();
    mesh.destroy();
    constexpr float phases[] { 0.f, 0.2f, 0.6f, 0.65f, 1.5f };
    constexpr float amps[] { 0.1f, 0.9f, 0.7f, 0.7f, 0.f };
    for (int index = 0; index < 5; ++index) {
        auto* cube = new VertCube(&mesh);
        for (int corner = 0; corner < (int) VertCube::numVerts; ++corner) {
            bool time, red, blue;
            VertCube::getPoles(corner, time, red, blue);
            auto& values = cube->getVertex(corner)->values;
            values[Vertex::Time] = time ? 1.f : 0.f;
            values[Vertex::Red] = red ? 1.f : 0.f;
            values[Vertex::Blue] = blue ? 1.f : 0.f;
            values[Vertex::Phase] = phases[index];
            values[Vertex::Amp] = amps[index];
            values[Vertex::Curve] = 0.5f;
        }
        mesh.addCube(cube);
    }
    mesh.loopCubes.insert(mesh.getCubes()[1]);
    mesh.sustainCubes.insert(mesh.getCubes()[2]);
    REQUIRE(model.synchronizeFromMesh(nullptr));
    return EnvelopeSignalProcessor::buildConfiguration(
            { { "purpose", "Purpose", "scratch" } },
            CurveNodeModelState::copyOf(model, 0.f, 0.5f, model.revision()));
}

}

TEST_CASE("Envelope cursor exposes loop wrapping and note-off release jump",
        "[cycle-v2][envelope][playback-cursor]") {
    const auto configuration = loopEnvelope();
    REQUIRE(configuration != nullptr);
    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "scratch", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 8;
    spec.sampleRate = 100.;
    processor.prepareExecution(spec);
    PlaybackCursorCapture cursor;
    cursor.nodeKey = (uint64_t) String("scratch").hashCode64();
    AudioProcessContext context;
    context.nodeKey = cursor.nodeKey;
    context.frameCount = 8;
    context.captureTraversalGrid = false;
    context.timing.sampleRate = spec.sampleRate;
    context.voice.playbackCursor = &cursor;
    context.voice.controls.normalizedVoiceTimeIncrement = 0.01;
    context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
    context.voice.events = { { NoteLifecycleType::NoteOn, 0, 0 } };
    processor.process(context);
    REQUIRE(cursor.hasEnvelopeTime);
    REQUIRE(cursor.envelopeTime == Catch::Approx(processor.playbackPosition()));
    context.voice.events.clear();
    float last = cursor.envelopeTime;
    bool wrapped = false;
    for (int block = 0; block < 12; ++block) {
        processor.process(context);
        wrapped |= cursor.envelopeTime < last;
        last = cursor.envelopeTime;
        REQUIRE(cursor.envelopeTime <= 0.61f);
    }
    REQUIRE(wrapped);
    context.voice.events = { { NoteLifecycleType::NoteOff, 0, 0 } };
    processor.process(context);
    REQUIRE(cursor.hasEnvelopeTime);
    REQUIRE(cursor.envelopeTime >= 0.6f);
    REQUIRE(cursor.envelopeTime == Catch::Approx(processor.playbackPosition()));
}

TEST_CASE("Mesh playback cursor records the resolved scratch coordinate",
        "[cycle-v2][runtime][playback-cursor]") {
    PlaybackCursorCapture cursor;
    TrimeshMorphInputs inputs;
    inputs.hasScratchOverride = true;
    inputs.scratchOverride = 0.73f;
    inputs.playbackCursor = &cursor;
    TrimeshMorphResolver resolver;
    const MorphPosition position = resolver.resolve(inputs, { 0.15f, 0.5f, 0.5f },
            PortDomain::TimeSignal, Vertex::Time, 0, 64, 48000., true);
    REQUIRE(cursor.hasMorph);
    REQUIRE(cursor.morph[0] == Catch::Approx(position.time.getCurrentValue()));
    REQUIRE(cursor.morph[0] == Catch::Approx(0.73f));
    inputs.scratchOverride = 0.22f;
    resolver.resolve(inputs, { 0.15f, 0.5f, 0.5f },
            PortDomain::TimeSignal, Vertex::Time, 0, 64, 48000., true);
    REQUIRE(cursor.morph[0] == Catch::Approx(0.22f));
}

TEST_CASE("Envelope overlays keep Phase horizontal and morph vertical",
        "[cycle-v2][ui][playback-cursor]") {
    PanelPlaybackOverlay overlay;
    PlaybackCursorFrame frame;
    frame.count = 2;
    for (auto& voice : frame.voices) {
        voice.setMorph({ 0.1f, 0.2f, 0.8f });
        voice.envelopeTime = 1.2f;
        voice.hasEnvelopeTime = true;
    }
    overlay.setFrame(frame);
    DynamicObject state;
    overlay.appendAutomation(state, Vertex::Blue, true, true);
    auto* cursors = state.getProperty("playbackCursors").getArray();
    REQUIRE(cursors != nullptr);
    REQUIRE(cursors->size() == 4);
    REQUIRE((*cursors)[0]["axis"].toString() == "x");
    REQUIRE((float) (*cursors)[0]["position"] == Catch::Approx(1.2f));
    REQUIRE((*cursors)[1]["axis"].toString() == "y");
    REQUIRE((float) (*cursors)[1]["position"] == Catch::Approx(0.8f));
    overlay.setFrame({});
    overlay.setSlice(0.43f);
    overlay.appendAutomation(state, Vertex::Blue, true, true);
    REQUIRE(state.getProperty("playbackCursors").getArray()->size() == 1);
    REQUIRE((float) state.getProperty("playbackCursors")[0]["position"] == Catch::Approx(0.43f));
}

TEST_CASE("Spy grids hold their x=1 waveshape throughout release columns",
        "[cycle-v2][preview][playback-cursor]") {
    const PreviewTraversal traversal(101);
    REQUIRE(traversal.morphPosition(0) == 0.f);
    REQUIRE(traversal.morphPosition(40) == Catch::Approx(0.5f));
    REQUIRE(traversal.morphPosition(80) == 1.f);
    REQUIRE(traversal.beginsRelease(81));
    for (size_t column = 81; column < 101; ++column) {
        REQUIRE(traversal.morphPosition(column) == 1.f);
    }
}

TEST_CASE("Spy envelope traversal leaves its sustain loop for release",
        "[cycle-v2][envelope][preview][playback-cursor]") {
    const auto configuration = loopEnvelope();
    REQUIRE(configuration != nullptr);
    EnvelopeSignalProcessor processor;
    processor.adoptConfiguration({ 1, "scratch", configuration });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 8;
    spec.traversalColumnCount = 101;
    processor.prepareExecution(spec);
    AudioProcessContext context;
    context.frameCount = 8;
    context.traversalColumnCount = 101;
    context.outputPorts = { { "env", PortDomain::EnvelopeSignal, ChannelLayout::Mono } };
    context.voice.events = { { NoteLifecycleType::NoteOn, 0, 0 } };
    processor.process(context);
    REQUIRE(context.outputs.size() == 1);
    const auto& grid = context.outputs.front().traversalGrid;
    REQUIRE(grid.isValid());
    REQUIRE(grid.columns == 101);
    const float releaseStart = grid.values[81 * grid.rows];
    const float releaseEnd = grid.values[100 * grid.rows];
    REQUIRE(releaseEnd < releaseStart - 0.05f);
    REQUIRE(releaseEnd < 0.7f);
}
