#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphSerializer.h"
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
    REQUIRE(started.leftPeak > 0.f);
    REQUIRE(started.rightPeak > 0.f);
    REQUIRE(started.peak == juce::jmax(started.leftPeak, started.rightPeak));

    REQUIRE(queue.enqueue(
            MidiMessage::noteOff(1, 60),
            MidiEventSource::PerformanceKeyboard,
            1.1));
    renderer.process(queue, channels, 2, 256, 44100.0, 1.1);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);

    double callbackTime = 1.1 + 256.0 / 44100.0;
    for (int block = 0;
            block < 64 && renderer.diagnostics(queue).activeVoiceCount > 0;
            ++block) {
        renderer.process(queue, channels, 2, 256, 44100.0, callbackTime);
        callbackTime += 256.0 / 44100.0;
    }
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);
}

TEST_CASE("Realtime graph renderer stops immediately without a volume envelope",
        "[cycle-v2][audio-device][realtime][midi][release]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeNode("env");
    graph.removeNode("multiply");
    graph.addEdge({
            "ifft", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 256;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 18, spec);
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
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);

    REQUIRE(queue.enqueue(
            MidiMessage::noteOff(1, 60),
            MidiEventSource::PerformanceKeyboard,
            1.1));
    renderer.process(queue, channels, 2, 256, 44100.0, 1.1);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);
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

TEST_CASE("Fully released spectral notes repeat on the same voice instance",
        "[cycle-v2][audio-device][realtime][midi][spectral-frame][repeat-note]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("filter-saw.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());

    constexpr int frameCount = 256;
    constexpr double sampleRate = 48000.0;
    constexpr double blockDuration = frameCount / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 23, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    renderer.setVoiceDurationSeconds(1.f);
    AudioBuffer<float> output(2, frameCount);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    double callbackTime = 1.0;

    const auto renderNote = [&] {
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 72, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                callbackTime));
        std::vector<float> samples;
        samples.reserve(8 * frameCount);
        for (int block = 0; block < 8; ++block) {
            renderer.process(
                    queue,
                    channels,
                    2,
                    frameCount,
                    sampleRate,
                    callbackTime);
            samples.insert(
                    samples.end(),
                    output.getReadPointer(0),
                    output.getReadPointer(0) + frameCount);
            callbackTime += blockDuration;
        }
        return samples;
    };

    const std::vector<float> first = renderNote();
    REQUIRE(queue.enqueue(
            MidiMessage::noteOff(1, 72),
            MidiEventSource::PerformanceKeyboard,
            callbackTime));
    for (int block = 0;
            block < 256 && renderer.diagnostics(queue).activeVoiceCount > 0;
            ++block) {
        renderer.process(
                queue,
                channels,
                2,
                frameCount,
                sampleRate,
                callbackTime);
        callbackTime += blockDuration;
    }
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);

    const std::vector<float> second = renderNote();
    REQUIRE(second == first);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
