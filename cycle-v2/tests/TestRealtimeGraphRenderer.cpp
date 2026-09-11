#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/NodeParameterMap.h"
#include "Graph/GraphSerializer.h"
#include "Graph/NodeGraph.h"
#include "Runtime/RealtimeGraphRenderer.h"

using namespace CycleV2;
using namespace juce;

namespace {

std::vector<float> renderOutputGain(float gainUnitValue, float& compiledGain) {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "out",
            "gain",
            "Gain",
            String(gainUnitValue)).succeeded());

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    compiledGain = compiled.plan.outputGain;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 256;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            1.0));

    AudioBuffer<float> outputBuffer(2, 256);
    float* channels[] {
            outputBuffer.getWritePointer(0),
            outputBuffer.getWritePointer(1)
    };
    renderer.process(queue, channels, 2, 256, 44100.0, 1.0);
    return {
            outputBuffer.getReadPointer(0),
            outputBuffer.getReadPointer(0) + outputBuffer.getNumSamples()
    };
}

}

TEST_CASE("Realtime graph renderer applies Output gain separately from safety headroom",
        "[cycle-v2][audio-device][realtime][output][gain]") {
    float quietGain {};
    float unityGain {};
    const auto quiet = renderOutputGain(0.4f, quietGain);
    const auto unity = renderOutputGain(0.5f, unityGain);

    REQUIRE(quietGain == Catch::Approx(CycleDsp::outputGain(0.4f)));
    REQUIRE(unityGain == Catch::Approx(1.f));
    float maximumResidual = 0.f;
    for (size_t index = 0; index < quiet.size(); ++index) {
        maximumResidual = std::max(
                maximumResidual,
                std::abs(quiet[index] - unity[index] * quietGain));
    }
    REQUIRE(maximumResidual < 1e-6f);
}

TEST_CASE("Realtime Output gain changes do not replace the graph or active voice",
        "[cycle-v2][audio-device][realtime][output][gain][gesture]") {
    const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compiled.succeeded());
    constexpr int frameCount = 256;
    constexpr double sampleRate = 44'100.0;
    constexpr double blockDuration = frameCount / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 41, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    renderer.setVoiceDurationSeconds(2.f);
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            1.0));
    AudioBuffer<float> output(2, frameCount);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, frameCount, sampleRate, 1.0);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);

    renderer.setGraphOutputGain(CycleDsp::outputGain(0.f));
    double callbackTime = 1.0 + blockDuration;
    for (int block = 0; block < 8; ++block) {
        renderer.process(
                queue,
                channels,
                2,
                frameCount,
                sampleRate,
                callbackTime);
        callbackTime += blockDuration;
    }

    const auto quiet = renderer.diagnostics(queue);
    REQUIRE(quiet.graphRevision == 41);
    REQUIRE(quiet.activeVoiceCount == 1);
    REQUIRE(quiet.peak > 0.f);
    REQUIRE(quiet.peak < 0.02f);
}

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
    renderer.setVoiceDurationSeconds(1.f);

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

TEST_CASE("Realtime graph renderer supplies the legacy volume-envelope clock",
        "[cycle-v2][audio-device][realtime][envelope][internal-rate][parity]") {
    const auto render = [](double volumeEnvelopeSampleRate) {
        const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
        REQUIRE(compiled.succeeded());

        AudioExecutionSpec spec;
        spec.maximumFrameCount = 256;
        spec.sampleRate = 44'100.;
        auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 19, spec);
        RealtimeGraphRenderer renderer;
        RealtimeMidiEventQueue queue;
        renderer.setPreparedGraph(prepared.get());
        renderer.setVoiceDurationSeconds(0.02f);
        renderer.setVolumeEnvelopeClockSampleRate(volumeEnvelopeSampleRate);
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 60, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                1.0));

        AudioBuffer<float> output(2, 256);
        float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
        renderer.process(queue, channels, 2, 256, 44'100., 1.0);
        return std::vector<float>(
                output.getReadPointer(0),
                output.getReadPointer(0) + output.getNumSamples());
    };

    const auto internalClock = render(44'100.);
    const auto outputClock = render(48'000.);
    REQUIRE(Buffer<float>(
            const_cast<float*>(internalClock.data()),
            (int) internalClock.size()).normDiffL2({
                const_cast<float*>(outputClock.data()),
                (int) outputClock.size()
            }) > 0.001f);
}

TEST_CASE("Realtime voice length updates the active voice-time clock",
        "[cycle-v2][audio-device][realtime][voice-length]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "time", {}));
    graph.replaceNodeParameters("time", {
            { "source", "Source", "voiceTime" },
            { "controller", "Controller", "1" },
            { "constant", "Constant", "0.5" }
    });
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "time", "value", "out", "time",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    constexpr int frameCount = 10;
    constexpr double sampleRate = 1'000.0;
    constexpr double blockDuration = frameCount / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 31, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    renderer.setVoiceDurationSeconds(1.f);

    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            1.0));
    AudioBuffer<float> output(2, frameCount);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, frameCount, sampleRate, 1.0);
    const float slowDelta = output.getSample(0, frameCount - 1)
            - output.getSample(0, 0);

    renderer.setVoiceDurationSeconds(0.1f);
    renderer.process(
            queue,
            channels,
            2,
            frameCount,
            sampleRate,
            1.0 + blockDuration);
    const float fastDelta = output.getSample(0, frameCount - 1)
            - output.getSample(0, 0);

    REQUIRE(slowDelta > 0.f);
    REQUIRE(fastDelta == Catch::Approx(10.f * slowDelta).margin(1.0e-6f));
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

TEST_CASE("Global delay continues after its source voice retires",
        "[cycle-v2][audio-device][realtime][delay][audio-scope]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeNode("env");
    graph.removeNode("multiply");
    GraphNodeFactory factory;
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", {}));
    graph.replaceNodeParameters("delay", {
            { "enabled", "Enabled", "1" },
            { "time", "Time", "0" },
            { "feedback", "Feedback", "0.5" },
            { "wet", "Wet", "1" },
            { "spin", "Pan Amount", "0" },
            { "spinIters", "Pan Cycle", "0" }
    });
    graph.addEdge({
            "ifft", "time", "delay", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delay", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    constexpr int frameCount = 256;
    constexpr double sampleRate = 44'100.0;
    constexpr double blockDuration = frameCount / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 29, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());

    double callbackTime = 1.0;
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            callbackTime));
    AudioBuffer<float> output(2, frameCount);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(
            queue,
            channels,
            2,
            frameCount,
            sampleRate,
            callbackTime);

    callbackTime += blockDuration;
    REQUIRE(queue.enqueue(
            MidiMessage::noteOff(1, 60),
            MidiEventSource::PerformanceKeyboard,
            callbackTime));

    float releasedPeak = 0.f;
    for (int block = 0; block < 12; ++block) {
        renderer.process(
                queue,
                channels,
                2,
                frameCount,
                sampleRate,
                callbackTime);
        REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);
        releasedPeak = jmax(releasedPeak, renderer.diagnostics(queue).peak);
        callbackTime += blockDuration;
    }

    REQUIRE(releasedPeak > 1.0e-4f);
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
