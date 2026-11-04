#include <catch2/catch_test_macros.hpp>

#include "App/StandaloneAudioEngine.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeStateEditor.h"
#include "UI/PerformanceKeyboard.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Engine source release clears every legato MIDI channel and preserves other sources",
        "[cycle-v2][audio-engine][realtime][legato]") {
    ScopedJuceInitialiser_GUI initialiseJuce;
    for (const auto source : { MidiEventSource::Hardware,
            MidiEventSource::PerformanceKeyboard, MidiEventSource::PatternPlayback }) {
        NodeGraph graph = NodeGraph::createDemoGraph();
        graph.removeNode("env");
        graph.removeNode("multiply");
        graph.addEdge({ "ifft", "time", "voiceOutput", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal });
        REQUIRE(GraphNodeStateEditor().setNodeParameter(
                graph, "voice", "noteMode", "Note Mode", "legato").succeeded());
        const auto compiled = GraphCompiler().compile(graph);
        REQUIRE(compiled.succeeded());
        StandaloneAudioEngine engine;
        REQUIRE(engine.publishGraph(compiled.plan, 1));
        const auto otherSource = source == MidiEventSource::Hardware
                ? MidiEventSource::PerformanceKeyboard : MidiEventSource::Hardware;
        REQUIRE(engine.enqueueMidiMessageAt(MidiMessage::noteOn(2, 60, (uint8) 100), source, 0.));
        REQUIRE(engine.enqueueMidiMessageAt(MidiMessage::noteOn(7, 64, (uint8) 100), source, 0.));
        REQUIRE(engine.enqueueMidiMessageAt(MidiMessage::noteOn(3, 67, (uint8) 100), otherSource, 0.));
        AudioBuffer<float> output(2, 256);
        float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
        const auto callback = [&] {
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 256, {});
        };
        callback();
        REQUIRE(engine.status().renderer.activeVoiceCount == 3);
        engine.releaseMidiSource(source);
        callback();
        REQUIRE(engine.status().renderer.activeVoiceCount == 1);
        engine.releaseMidiSource(otherSource);
        callback();
        REQUIRE(engine.status().renderer.activeVoiceCount == 0);
    }
}

TEST_CASE("Seeking a pattern cancels future old-generation notes in the real engine",
        "[cycle-v2][audio-engine][realtime][sequence][seek]") {
    ScopedJuceInitialiser_GUI gui;
    const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
    REQUIRE(compiled.succeeded());
    StandaloneAudioEngine engine;
    REQUIRE(engine.publishGraph(compiled.plan, 1));
    MidiKeyboardState keyboard;
    PerformanceKeyboardPanel panel(keyboard, engine);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes = {{60, 100, 0.0, 0.5}, {67, 90, 2.0, 0.5}};
    panel.setSequence(phrase);
    const double now = Time::getMillisecondCounterHiRes();
    REQUIRE(panel.startPlayback(now));
    AudioBuffer<float> output(2, 256);
    float* channels[] {output.getWritePointer(0), output.getWritePointer(1)};
    const auto callback = [&] {
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, channels, 2, 256, {});
    };
    callback();
    REQUIRE(engine.status().renderer.activeVoiceCount == 1);
    REQUIRE(engine.enqueueMidiMessage(MidiMessage::noteOn(1, 64, (uint8) 90), MidiEventSource::Hardware));
    callback();
    REQUIRE(engine.status().renderer.activeVoiceCount == 2);
    engine.resetAudioPerformance();
    panel.seekPlayback(3.0, Time::getMillisecondCounterHiRes());
    callback();
    REQUIRE(engine.status().renderer.activeVoiceCount == 1);
    const auto performance = engine.audioPerformance();
    REQUIRE((bool) performance["enabled"]);
    REQUIRE((int) performance["callbackCount"] == 1);
    REQUIRE(performance["workload"].hasProperty("maximumScheduledMidiEventCount"));
    REQUIRE((int) performance["workload"]["maximumScheduledMidiEventCount"] == 0);
}
