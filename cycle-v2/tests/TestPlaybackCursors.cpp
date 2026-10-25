#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/VertCube.h>

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Runtime/PreviewTraversal.h"
#include "Graph/GraphCompiler.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/TrimeshMorphResolver.h"
#include "UI/PanelPlaybackOverlay.h"
#include "UI/SignalProbeDetailView.h"
#include "Runtime/DefaultOutputPreview.h"

using namespace CycleV2;
using namespace juce;

namespace {

std::shared_ptr<const EnvelopeConfiguration> loopEnvelope(
        float end = 1.5f, float blueEnd = -1.f, bool enabled = true) {
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
            values[Vertex::Phase] = index == 4 ? (blue && blueEnd > 0.f ? blueEnd : end) : phases[index];
            values[Vertex::Amp] = amps[index];
            values[Vertex::Curve] = 0.5f;
        }
        mesh.addCube(cube);
    }
    mesh.loopCubes.insert(mesh.getCubes()[1]);
    mesh.sustainCubes.insert(mesh.getCubes()[2]);
    REQUIRE(model.synchronizeFromMesh(nullptr));
    return EnvelopeSignalProcessor::buildConfiguration(
            { { "purpose", "Purpose", "scratch" }, { "dimensionCount", "Dimensions", "3" },
                    { "enabled", "Enabled", enabled ? "1" : "0" } },
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
    const PreviewTraversal traversal(101, 0.25);
    REQUIRE(traversal.morphPosition(0) == 0.f);
    REQUIRE(traversal.morphPosition(40) == Catch::Approx(0.5f));
    REQUIRE(traversal.morphPosition(80) == 1.f);
    REQUIRE(traversal.beginsRelease(80));
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
    const double duration = Rasterization::EnvelopePlaybackEngine::releaseDuration(
            configuration->rasterizer->preparedPlaybackView());
    REQUIRE(grid.metadata.releaseDuration == Catch::Approx(duration));
    const PreviewTraversal traversal(grid.columns, duration);
    size_t noteOff = 0;
    while (!traversal.beginsRelease(noteOff)) {
        ++noteOff;
    }
    const float releaseStart = grid.values[noteOff * grid.rows];
    const float releaseEnd = grid.values[100 * grid.rows];
    REQUIRE(releaseEnd < releaseStart - 0.05f);
    REQUIRE(releaseEnd == Catch::Approx(0.f).margin(0.005f));
}

TEST_CASE("Spy timeline fits both short and long complete releases",
        "[cycle-v2][preview][release-timing]") {
    for (const double release : { 0., 0.05, 0.85, 2. }) {
        for (const size_t columns : { 8u, 101u, 512u }) {
            const PreviewTraversal traversal(columns, release);
            double elapsed = 0.;
            size_t noteOff = columns - 1;
            for (size_t column = 0; column + 1 < columns; ++column) {
                if (traversal.beginsRelease(column)) {
                    REQUIRE(elapsed == Catch::Approx(1.));
                    noteOff = column;
                }
                elapsed += traversal.increment(column);
            }
            REQUIRE(elapsed == Catch::Approx(1. + release));
            REQUIRE((double) noteOff / (double) (columns - 1)
                    == Catch::Approx(1. / (1. + release)).margin(1. / (double) (columns - 1)));
            REQUIRE(traversal.noteOffPosition()
                    == Catch::Approx((double) noteOff / (double) (columns - 1)));
            for (size_t column = noteOff; column < columns; ++column) {
                REQUIRE(traversal.morphPosition(column) == 1.f);
            }
        }
    }
}

TEST_CASE("Envelope tail measurement uses its actual release entry policy",
        "[cycle-v2][envelope][release-timing]") {
    const auto configuration = loopEnvelope();
    auto prepared = configuration->rasterizer->preparedPlaybackView();
    // The mature rasterizer inserts a sustain floor point at 0.6001 for unipolar release.
    REQUIRE(Rasterization::EnvelopePlaybackEngine::releaseDuration(prepared) == Catch::Approx(0.8999));
    prepared.scalingMode = Rasterization::PointScalingMode::Bipolar;
    REQUIRE(Rasterization::EnvelopePlaybackEngine::releaseDuration(prepared) == Catch::Approx(0.9));
    prepared.sustainIndex = (int) prepared.display.intercepts.size() - 1;
    REQUIRE(Rasterization::EnvelopePlaybackEngine::releaseDuration(prepared) == 0.);
}

namespace {

void setEnvelopeTiming(GraphExecutionPlan& plan, const String& id,
        std::shared_ptr<const EnvelopeConfiguration> configuration) {
    const int index = plan.dependencyIndex.stepIndexById.at(id);
    auto& published = plan.steps[(size_t) index].configuration;
    published.value = std::move(configuration);
    ++published.revision;
}

const SignalTraversalGrid& envelopeGrid(const GraphAudioResult& result, const String& id) {
    const auto found = std::find_if(result.nodes.begin(), result.nodes.end(),
            [&](const auto& node) { return node.nodeId == id; });
    REQUIRE(found != result.nodes.end());
    return found->output.traversalGrid;
}

}

TEST_CASE("All Spy captures share the longest preset envelope tail",
        "[cycle-v2][preview][release-timing]") {
    const auto graph = NodeGraph::createDemoGraph();
    auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    setEnvelopeTiming(compiled.plan, "env", loopEnvelope(0.8f));
    setEnvelopeTiming(compiled.plan, "scratchEnv", loopEnvelope(2.5f));
    GraphAudioExecutor executor;
    AudioVoiceContext voice;
    voice.events = { { NoteLifecycleType::NoteOn, 0, 0 } };
    const auto result = executor.process(graph, compiled.plan, 8, {}, voice, 201);
    REQUIRE(result.previewReleaseDuration == Catch::Approx(1.8999));
    const auto previews = GraphPreviewExecutor().render(
            compiled.plan, result, graph.getSignalProbes(), 201);
    REQUIRE(previews.defaultOutput.has_value());
    for (const bool spectrum : { false, true }) {
        auto preview = spectrum ? DefaultOutputPreview::spectrum(*previews.defaultOutput)
                                : *previews.defaultOutput;
        REQUIRE(preview.releaseDuration == Catch::Approx(result.previewReleaseDuration));
        const size_t columns = preview.gridColumns;
        SignalProbeDetailState detail;
        detail.open(std::move(preview), RenderScalePolicy::Unipolar, 0, 60, 201);
        size_t noteOff = 0;
        const PreviewTraversal traversal(columns, result.previewReleaseDuration);
        while (!traversal.beginsRelease(noteOff) && noteOff + 1 < columns) {
            ++noteOff;
        }
        REQUIRE(detail.noteOffPosition() == Catch::Approx((double) noteOff / (double) (columns - 1)));
    }
    size_t timedGrids = 0;
    for (const auto& node : result.nodes) {
        const auto& grid = node.output.traversalGrid;
        if (grid.isValid() && grid.metadata.columnAxis == TraversalGridAxis::Time) {
            REQUIRE(grid.metadata.releaseDuration == Catch::Approx(1.8999));
            ++timedGrids;
        }
    }
    REQUIRE(timedGrids > 2);
    for (const String& id : { "env", "scratchEnv" }) {
        const auto& grid = envelopeGrid(result, id);
        REQUIRE(grid.metadata.releaseDuration == Catch::Approx(1.8999));
        REQUIRE(grid.values[(grid.columns - 1) * grid.rows] == Catch::Approx(0.f).margin(0.005f));
    }
    // The longer scratch envelope is outside this scoped volume-envelope capture.
    const CompiledSignalProbe probe { "volume", compiled.plan.dependencyIndex.stepIndexById.at("env"), 0 };
    const auto scoped = executor.processProbe(compiled.plan, probe, 8, {}, voice, 201);
    REQUIRE(scoped.previewReleaseDuration == Catch::Approx(result.previewReleaseDuration));
    REQUIRE(envelopeGrid(scoped, "env").values == envelopeGrid(result, "env").values);

    setEnvelopeTiming(compiled.plan, "scratchEnv", loopEnvelope(2.5f, -1.f, false));
    const auto disabled = executor.process(graph, compiled.plan, 8, {}, voice, 201);
    REQUIRE(disabled.previewReleaseDuration == Catch::Approx(0.1999));
    setEnvelopeTiming(compiled.plan, "env", loopEnvelope(0.8f, -1.f, false));
    const auto bypassed = executor.process(graph, compiled.plan, 8, {}, voice, 201);
    REQUIRE(bypassed.previewReleaseDuration == 0.);
}

TEST_CASE("Morphed release timing invalidates every cached Spy timeline",
        "[cycle-v2][preview][release-timing]") {
    const auto graph = NodeGraph::createDemoGraph();
    auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    setEnvelopeTiming(compiled.plan, "env", loopEnvelope(0.8f));
    setEnvelopeTiming(compiled.plan, "scratchEnv", loopEnvelope(1.5f, 2.5f));
    GraphAudioExecutor executor;
    AudioVoiceContext voice;
    voice.events = { { NoteLifecycleType::NoteOn, 0, 0 } };
    std::vector<uint8_t> dirty(compiled.plan.steps.size(), 1);
    const auto before = executor.processIncrementalIndexed(graph, compiled.plan, 8, dirty, voice, {}, 201);
    REQUIRE(before.previewReleaseDuration == Catch::Approx(0.8999));
    GraphPreviewResult previews = GraphPreviewExecutor().render(compiled.plan, before, {}, 40);
    std::fill(dirty.begin(), dirty.end(), 0);
    dirty[(size_t) compiled.plan.dependencyIndex.stepIndexById.at("scratchEnv")] = 1;
    voice.controls.controllers[1] = 1.f;
    const auto cancelled = executor.processIncrementalIndexed(
            graph, compiled.plan, 8, dirty, voice, [] { return false; }, 201);
    REQUIRE(cancelled.cancelled);
    const CompiledSignalProbe scratch { "scratch",
            compiled.plan.dependencyIndex.stepIndexById.at("scratchEnv"), 0 };
    const auto scoped = executor.processProbe(compiled.plan, scratch, 8, {}, voice, 201);
    REQUIRE(scoped.previewReleaseDuration == Catch::Approx(1.8999));
    const auto after = executor.processIncrementalIndexed(graph, compiled.plan, 8, dirty, voice, {}, 201);
    REQUIRE(after.previewReleaseDuration == Catch::Approx(1.8999));
    const auto volume = std::find_if(after.nodes.begin(), after.nodes.end(),
            [](const auto* node) { return node->nodeId == "env"; });
    REQUIRE(volume != after.nodes.end());
    REQUIRE((*volume)->output.traversalGrid.metadata.releaseDuration == Catch::Approx(1.8999));
    GraphPreviewExecutor().renderIncremental(compiled.plan, after, {}, dirty, 40, previews);
    REQUIRE(previews.previewReleaseDuration == Catch::Approx(1.8999));
    REQUIRE(previews.renderedNodeCount > 1);
}
