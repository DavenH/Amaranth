#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Graph/NodeGraph.h"
#include "Runtime/RealtimeGraphRenderer.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Realtime graph renderer turns MIDI note gestures into graph audio",
        "[cycle-v2][audio-device][realtime][midi]") {
    const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compiled.succeeded());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 256;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 17, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());

    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            1.0));
    AudioBuffer<float> output(2, 256);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, 256, 44100.0, 1.0);

    const auto started = renderer.diagnostics(queue);
    REQUIRE(started.graphRevision == 17);
    REQUIRE(started.activeVoiceCount == 1);
    REQUIRE(started.peak > 0.f);
    REQUIRE(started.rms > 0.f);

    REQUIRE(queue.enqueue(
            MidiMessage::noteOff(1, 60),
            MidiEventSource::PerformanceKeyboard,
            1.1));
    renderer.process(queue, channels, 2, 256, 44100.0, 1.1);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);
}

TEST_CASE("Realtime graph renderer isolates voices and steals the oldest voice",
        "[cycle-v2][audio-device][realtime][midi]") {
    const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compiled.succeeded());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());

    for (size_t note = 0; note < RealtimeGraphRenderer::voiceCount + 1; ++note) {
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 48 + (int) note, (uint8) 100),
                MidiEventSource::Hardware,
                2.0));
    }

    AudioBuffer<float> output(2, 64);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, 64, 44100.0, 2.0);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == RealtimeGraphRenderer::voiceCount);
}

TEST_CASE("Realtime graph renderer defers events beyond the current callback",
        "[cycle-v2][audio-device][realtime][midi]") {
    const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compiled.succeeded());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            2.0));

    AudioBuffer<float> output(2, 64);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, 64, 44100.0, 1.0);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);

    renderer.process(queue, channels, 2, 64, 44100.0, 2.0);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);
    REQUIRE(renderer.diagnostics(queue).peak > 0.f);
}
