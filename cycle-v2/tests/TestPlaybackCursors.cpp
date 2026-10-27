#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/VertCube.h>

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "Nodes/Envelope/EnvelopePitchPreview.h"
#include "Runtime/PreviewTraversal.h"
#include "Runtime/GraphPresentationModel.h"
#include "Graph/GraphSerializer.h"
#include "Graph/GraphCompiler.h"
#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/TrimeshMorphResolver.h"
#include "UI/PanelPlaybackOverlay.h"
#include "Nodes/Envelope/Rendering/EnvelopeSurfacePanel.h"
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

TEST_CASE("Envelope surface dots follow time along their primary morph cut lines",
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
    REQUIRE(cursors->size() == 2);
    REQUIRE((*cursors)[0]["axis"].toString() == "y");
    REQUIRE((float) (*cursors)[0]["position"] == Catch::Approx(0.8f));
    const auto markers = state.getProperty("playbackDots");
    REQUIRE(markers.getArray()->size() == 2);
    REQUIRE((float) markers[0]["x"] == Catch::Approx(1.2f));
    REQUIRE((float) markers[0]["y"] == Catch::Approx(0.8f));
    frame.voices[1].setMorph({ 0.6f, 0.7f, 0.3f });
    frame.voices[1].envelopeTime = 0.4f;
    overlay.setFrame(frame);
    for (const auto axis : { Vertex::Time, Vertex::Red, Vertex::Blue }) {
        overlay.appendAutomation(state, axis, true, true);
        const auto lines = state.getProperty("playbackCursors");
        const auto dots = state.getProperty("playbackDots");
        REQUIRE(dots.getArray()->size() == 2);
        REQUIRE((float) dots[1]["x"] == Catch::Approx(0.4f));
        REQUIRE(dots[1]["y"] == lines[1]["position"]);
        REQUIRE(lines[1]["axis"].toString() == "y");
    }
    overlay.appendAutomation(state, Vertex::Red, true, false);
    REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
    REQUIRE(state.getProperty("playbackCursors")[0]["axis"].toString() == "x");
    overlay.appendAutomation(state, Vertex::Red, false, true);
    REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
    REQUIRE(state.getProperty("playbackCursors")[0]["axis"].toString() == "x");
    overlay.setFrame({});
    overlay.setSlice(0.43f);
    overlay.appendAutomation(state, Vertex::Blue, true, true);
    REQUIRE(state.getProperty("playbackCursors").getArray()->size() == 1);
    REQUIRE((float) state.getProperty("playbackCursors")[0]["position"] == Catch::Approx(0.43f));
    REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
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

TEST_CASE("Spy note-off auditions preserve the axis and hold the chosen morph slice",
        "[cycle-v2][preview][release-audition]") {
    constexpr size_t columns = 401;
    for (const double release : { 0., 0.2, 1.5 }) {
        const PreviewTraversal normal(columns, release);
        for (const double noteOff : { 0., 0.2, 0.7, 1. }) {
            const PreviewTraversal audition(columns, release, noteOff);
            double elapsed = 0.;
            size_t releases = 0;
            for (size_t column = 0; column < columns; ++column) {
                if (audition.beginsRelease(column)) {
                    REQUIRE(elapsed == Catch::Approx(noteOff).margin(0.01));
                    ++releases;
                }
                if (column + 1 < columns) {
                    REQUIRE(audition.increment(column) == normal.increment(column));
                    elapsed += audition.increment(column);
                }
            }
            REQUIRE(elapsed == Catch::Approx(1. + release));
            REQUIRE(releases == (noteOff < 1. || release > 0. ? 1 : 0));
            REQUIRE(audition.morphPosition(columns - 1) == Catch::Approx(noteOff).margin(0.01));
            REQUIRE(audition.noteOffPosition()
                    == Catch::Approx(normal.noteOffPosition() * noteOff).margin(1. / (double) (columns - 1)));
        }
    }
}

TEST_CASE("Scoped Spy auditions release envelopes early and do not leak into normal captures",
        "[cycle-v2][preview][release-audition]") {
    const auto graph = NodeGraph::createDemoGraph();
    auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    setEnvelopeTiming(compiled.plan, "env", loopEnvelope(0.8f));
    setEnvelopeTiming(compiled.plan, "scratchEnv", loopEnvelope(0.8f));
    const CompiledSignalProbe volume { "volume", compiled.plan.dependencyIndex.stepIndexById.at("env"), 0 };
    const CompiledSignalProbe scratch { "scratch", compiled.plan.dependencyIndex.stepIndexById.at("scratchEnv"), 0 };
    AudioVoiceContext voice;
    voice.events = { { NoteLifecycleType::NoteOn, 0, 0 } };
    GraphAudioExecutor executor;
    const auto normal = executor.processProbe(compiled.plan, volume, 8, {}, voice, 401);
    const auto early = executor.processProbes(compiled.plan, { volume, scratch }, 8, {}, voice, 401, 0.2);
    const auto& held = envelopeGrid(normal, "env");
    const auto& released = envelopeGrid(early, "env");
    REQUIRE(released.metadata.noteOffTime == 0.2);
    REQUIRE(released.metadata.releaseDuration == held.metadata.releaseDuration);
    REQUIRE(envelopeGrid(early, "scratchEnv").metadata.noteOffTime == 0.2);
    for (size_t column = 0; column < 50; ++column) {
        REQUIRE(released.values[column * released.rows] == held.values[column * held.rows]);
    }
    REQUIRE(released.values[200 * released.rows] == Catch::Approx(0.f).margin(0.005f));
    REQUIRE(held.values[200 * held.rows] > 0.2f);

    const auto probe = GraphPreviewExecutor().captureProbe(compiled.plan, early, volume);
    REQUIRE(probe.noteOffTime == 0.2);
    REQUIRE(DefaultOutputPreview::spectrum(probe).noteOffTime == 0.2);
    const std::vector<uint8_t> dirty(compiled.plan.steps.size(), 0);
    const auto restored = executor.processIncrementalIndexed(
            graph, compiled.plan, 8, dirty, voice, {}, 401);
    const auto restoredVolume = std::find_if(restored.nodes.begin(), restored.nodes.end(),
            [](const auto* node) { return node->nodeId == "env"; });
    REQUIRE(restoredVolume != restored.nodes.end());
    REQUIRE((*restoredVolume)->output.traversalGrid.metadata.noteOffTime == 1.);
    REQUIRE((*restoredVolume)->output.traversalGrid.values == held.values);
}

TEST_CASE("Spy release drags defer capture and preserve their grid through commit and cancel",
        "[cycle-v2][canvas][probe][detail][release-audition]") {
    GraphPreviewResult::SignalProbePreview preview;
    preview.probeId = "probe";
    preview.gridColumns = 512;
    preview.gridRows = 512;
    preview.values.resize(preview.gridColumns * preview.gridRows, 0.5f);
    preview.releaseDuration = 0.5;
    SignalProbeDetailState detail;
    detail.open(std::move(preview), RenderScalePolicy::Unipolar, 1, 60, 512);
    const Rectangle<float> bounds { 0.f, 0.f, 920.f, 620.f };
    const auto voice = SignalProbeDetailView::voiceBounds(bounds, detail);
    const Point<float> start = SignalProbeDetailView::releaseHandleBounds(bounds, detail).getCentre();
    const auto* storage = detail.renderResult.primary.data();
    const auto revision = detail.renderResult.contentRevision;
    REQUIRE(detail.beginReleaseDrag(bounds, start));
    for (const float movement : { -0.3f, -0.7f }) {
        detail.dragRelease(start.translated(movement * voice.getWidth(), 0.f));
        REQUIRE(detail.displayedNoteOffTime() == Catch::Approx(1.f + movement));
        REQUIRE(detail.noteOffTime == 1.);
        REQUIRE(detail.renderCount == 1);
        REQUIRE(detail.renderResult.primary.data() == storage);
        REQUIRE(detail.renderResult.contentRevision == revision);
    }
    REQUIRE(detail.endReleaseDrag(start.translated(-0.7f * voice.getWidth(), 0.f)));
    REQUIRE(detail.noteOffTime == Catch::Approx(0.3));
    REQUIRE(detail.renderCount == 1); // The input owner requests the one capture after commit.
    REQUIRE_FALSE(detail.endReleaseDrag(start));
    auto handle = SignalProbeDetailView::releaseHandleBounds(bounds, detail).getCentre();
    REQUIRE(detail.beginReleaseDrag(bounds, handle));
    REQUIRE_FALSE(detail.endReleaseDrag(handle));
    const auto icon = SignalProbeDetailView::releaseIconBounds(bounds, detail).getCentre();
    REQUIRE(detail.beginReleaseDrag(bounds, icon));
    detail.dragRelease(icon.translated(-2.f * voice.getWidth(), 0.f));
    REQUIRE(detail.displayedNoteOffTime() == 0.);
    REQUIRE(detail.cancelReleaseDrag());
    REQUIRE(detail.noteOffTime == Catch::Approx(0.3));
    REQUIRE_FALSE(detail.endReleaseDrag(icon));
    handle = SignalProbeDetailView::releaseHandleBounds(bounds, detail).getCentre();
    REQUIRE(detail.beginReleaseDrag(bounds, handle));
    REQUIRE(detail.endReleaseDrag(handle.translated(2.f * voice.getWidth(), 0.f)));
    REQUIRE(detail.noteOffTime == 1.);
    REQUIRE(detail.renderResult.primary.data() == storage);
}

TEST_CASE("Spy pitch lanes take release from the auditioned note-off",
        "[cycle-v2][preview][release-audition]") {
    const auto configuration = loopEnvelope(0.8f);
    const auto held = EnvelopePitchPreview::renderLanes(configuration, 2, 401, 123, 0.2);
    const auto early = EnvelopePitchPreview::renderLanes(configuration, 2, 401, 123, 0.2, 0.2);
    REQUIRE(early.size() == held.size());
    for (size_t lane = 0; lane < early.size(); ++lane) {
        REQUIRE(early[lane][200] == Catch::Approx(0.f).margin(0.005f));
        REQUIRE(held[lane][200] > 0.2f);
    }
}

TEST_CASE("Spy release auditions survive collapse and refresh without changing node previews",
        "[cycle-v2][preview][release-audition]") {
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content/presets/baroque-flute.cyclegraph");
    const auto graph = GraphSerializer().fromJsonString(preset.loadFileAsString());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    const auto nodes = presentation.previewResult().nodes;
    const auto audioRevision = presentation.audioPlanRevision();
    const auto compilations = presentation.compilationCount();
    auto captured = presentation.captureProbePreview(graph, "default-output", 512, 48, 0.35);
    REQUIRE(captured.has_value());
    presentation.commitProbePreview(std::move(*captured));
    REQUIRE(presentation.probeNoteOffTime("default-output") == 0.35);
    REQUIRE(presentation.probeNoteOffTime("another-spy") == 1.);
    REQUIRE(presentation.previewResult().defaultOutput->noteOffTime == 0.35);
    REQUIRE(presentation.previewResult().defaultOutputSpectrum->noteOffTime == 0.35);
    auto probeCapture = presentation.captureProbePreview(graph, "probe", 512, 48, 0.55);
    REQUIRE(probeCapture.has_value());
    presentation.commitProbePreview(std::move(*probeCapture));
    REQUIRE(presentation.probeNoteOffTime("probe") == 0.55);
    REQUIRE(presentation.previewResult().probes.front().noteOffTime == 0.55);
    REQUIRE(presentation.previewResult().probeSpectra.front()->noteOffTime == 0.55);
    REQUIRE(presentation.probeNoteOffTime("default-output") == 0.35);
    for (size_t index = 0; index < nodes.size(); ++index) {
        REQUIRE(nodePreviewResultsHaveEqualContent(nodes[index], presentation.previewResult().nodes[index]));
    }
    REQUIRE(presentation.audioPlanRevision() == audioRevision);
    REQUIRE(presentation.compilationCount() == compilations);

    REQUIRE(presentation.refreshPreviewMidiNote(graph, 1, 60));
    REQUIRE(presentation.previewResult().defaultOutput->noteOffTime == 0.35);
    REQUIRE(presentation.previewResult().defaultOutputSpectrum->noteOffTime == 0.35);
    captured = presentation.captureProbePreview(graph, "default-output", 512, 60, 1.);
    REQUIRE(captured.has_value());
    presentation.commitProbePreview(std::move(*captured));
    REQUIRE(presentation.probeNoteOffTime("default-output") == 1.);
    REQUIRE(presentation.snapshot().probeNoteOffTimes.count("default-output") == 0);
    REQUIRE(presentation.probeNoteOffTime("probe") == 0.55);

    captured = presentation.captureProbePreview(graph, "default-output", 512, 60, 0.2);
    REQUIRE(captured.has_value());
    presentation.commitProbePreview(std::move(*captured));
    presentation.setProbeNoteOffTimes({});
    GraphChangeSet load;
    load.topologyChanged = true;
    REQUIRE(presentation.refresh(graph, 2, load));
    REQUIRE(presentation.probeNoteOffTime("default-output") == 1.);
    REQUIRE(presentation.previewResult().defaultOutput->noteOffTime == 1.);
}

TEST_CASE("Envelope surface Blues colours follow morph-dependent loop and release sections",
        "[cycle-v2][envelope][morph-surface][section-colours]") {
    const std::vector<float> values(9 * 3, 0.35f);
    const std::vector<Rasterization::EnvelopeSurfaceMarkers> markers {
        { 0.25f, 1.f, 0.f, 1.5f }, { 0.5f, 1.25f, 0.f, 1.75f }, { -1.f, 1.5f, 0.25f, 2.f }
    };
    const auto image = EnvelopeSurfacePanel::createColourMap(values, 9, 3, { 0.f, 2.f }, markers);
    REQUIRE(image.isValid());
    const auto blue = image.getPixelAt(0, 2);
    const auto yellow = image.getPixelAt(1, 2);
    const auto red = image.getPixelAt(4, 2);
    REQUIRE(blue.getBlue() > blue.getRed());
    REQUIRE(yellow.getHue() == Catch::Approx(0.14f).margin(0.01f));
    REQUIRE(red.getHue() == Catch::Approx(0.f).margin(0.01f));
    REQUIRE(yellow.getBrightness() == blue.getBrightness());
    REQUIRE(red.getBrightness() == blue.getBrightness());
    REQUIRE(image.getPixelAt(1, 1) == blue);
    REQUIRE(image.getPixelAt(2, 1) == yellow);
    REQUIRE(image.getPixelAt(4, 1) == yellow);
    REQUIRE(image.getPixelAt(5, 1) == red);
    REQUIRE(image.getPixelAt(5, 0) == blue);
    REQUIRE(image.getPixelAt(6, 0) == red);
    REQUIRE(TrimeshRenderProfile::fromDomain(PortDomain::EnvelopeSignal)
            .getSurfaceStyle().surfaceMaterial().palette == ScalarSurfacePalette::LegacyBlue);
}

TEST_CASE("Envelope surfaces leave missing tails empty and pitch remains bipolar",
        "[cycle-v2][envelope][morph-surface][section-colours]") {
    const std::vector<Rasterization::EnvelopeSurfaceMarkers> markers {
        { -1.f, std::numeric_limits<float>::max(), 0.f, 1.f },
        { 0.25f, 1.f, 0.f, 1.5f },
        {}
    };
    std::vector<float> values(9 * 3, 0.5f);
    values[0] = 0.47f;
    values[3] = 0.53f;
    const auto blue = EnvelopeSurfacePanel::createColourMap(values, 9, 3, { 0.f, 2.f }, markers);
    const auto pitch = EnvelopeSurfacePanel::createColourMap(values, 9, 3, { 0.f, 2.f }, markers, true);
    REQUIRE(blue.getPixelAt(4, 2).getAlpha() == 255);
    REQUIRE(blue.getPixelAt(5, 2).isTransparent());
    REQUIRE(blue.getPixelAt(6, 1).getAlpha() == 255);
    REQUIRE(blue.getPixelAt(7, 1).isTransparent());
    REQUIRE(blue.getPixelAt(0, 0).isTransparent());
    REQUIRE(pitch.getPixelAt(5, 2).isTransparent());
    REQUIRE(pitch.getPixelAt(7, 1).isTransparent());
    REQUIRE(pitch.getPixelAt(0, 0).isTransparent());
    const auto negative = pitch.getPixelAt(0, 2);
    const auto positive = pitch.getPixelAt(1, 2);
    REQUIRE(negative.getBlue() > negative.getRed());
    REQUIRE(positive.getRed() > positive.getBlue());
    REQUIRE(pitch.getPixelAt(2, 2) == pitch.getPixelAt(2, 1));
    REQUIRE(pitch.getPixelAt(4, 2) == pitch.getPixelAt(4, 1));
}

TEST_CASE("Trimesh keyboard diamonds stay on the primary axis until voices end",
        "[cycle-v2][ui][playback-cursor]") {
    PanelPlaybackOverlay overlay;
    PlaybackCursorFrame frame;
    frame.count = 2;
    frame.voices[0].setMorph({ 0.1f, 0.25f, 0.8f });
    frame.voices[1].setMorph({ 0.6f, 0.75f, 0.3f });
    overlay.setFrame(frame);
    DynamicObject state;
    for (const int axis : MorphDimensions::axes) {
        overlay.setKeyScaleAxis(axis);
        overlay.appendAutomation(state, axis, false, true);
        const auto markers = state.getProperty("playbackDots");
        const auto lines = state.getProperty("playbackCursors");
        REQUIRE(markers.getArray()->size() == 2);
        REQUIRE(state.getProperty("playbackMarkerAnchor").toString() == "primaryAxis");
        for (int voice = 0; voice < 2; ++voice) {
            REQUIRE(markers[voice]["x"] == lines[voice]["position"]);
            REQUIRE((float) markers[voice]["y"] == 0.f);
        }
        frame.voices[0].morph[axis == Vertex::Time ? 1 : 0] += 0.05f;
        frame.voices[0].envelopeTime += 0.1f;
        frame.voices[0].hasEnvelopeTime = true;
        overlay.setFrame(frame);
        overlay.appendAutomation(state, axis, false, true);
        REQUIRE(state.getProperty("playbackDots")[0]["x"] == markers[0]["x"]);
        overlay.appendAutomation(state, axis == Vertex::Red ? Vertex::Blue : Vertex::Red, false, true);
        REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
        overlay.setKeyScaleAxis(-1);
        overlay.appendAutomation(state, axis, false, true);
        REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
    }
    overlay.setKeyScaleAxis(Vertex::Red);
    frame.count = 1;
    frame.voices[0] = frame.voices[1];
    overlay.setFrame(frame);
    overlay.appendAutomation(state, Vertex::Red, false, true);
    REQUIRE(state.getProperty("playbackDots").getArray()->size() == 1);
    REQUIRE((float) state.getProperty("playbackDots")[0]["x"] == Catch::Approx(0.75f));
    overlay.setFrame({});
    overlay.appendAutomation(state, Vertex::Red, false, true);
    REQUIRE(state.getProperty("playbackDots").getArray()->isEmpty());
    REQUIRE(state.getProperty("playbackCursors").getArray()->size() == 1);
}
