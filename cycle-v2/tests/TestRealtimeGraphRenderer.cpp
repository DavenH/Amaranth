#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <Audio/CycleDsp/EffectParameterMapping.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeStateEditor.h"
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
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
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

std::vector<float> renderAstralRealtimeNote(int midiNote) {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("astral.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());

    constexpr int blockSize = 512;
    constexpr int sampleCount = 8192;
    constexpr double sampleRate = 44'100.0;
    constexpr double blockDuration = blockSize / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = blockSize;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, midiNote, (uint8) 102),
            MidiEventSource::PerformanceKeyboard,
            1.0));

    AudioBuffer<float> output(2, blockSize);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    std::vector<float> samples;
    samples.reserve(sampleCount);
    double callbackTime = 1.0;
    for (int start = 0; start < sampleCount; start += blockSize) {
        renderer.process(
                queue,
                channels,
                2,
                blockSize,
                sampleRate,
                callbackTime);
        samples.insert(
                samples.end(),
                output.getReadPointer(0),
                output.getReadPointer(0) + blockSize);
        callbackTime += blockDuration;
    }
    return samples;
#else
    ignoreUnused(midiNote);
    return {};
#endif
}

std::vector<float> renderAttachedVelocityMapping(
        const String& source,
        float constant,
        uint8 velocity) {
#if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("bright-lead-3.cyclegraph");
    GraphLoadResult loaded = GraphSerializer().loadJsonString(preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            loaded.graph,
            "morph",
            "blueSource",
            "Blue Source",
            source).succeeded());
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            loaded.graph,
            "morph",
            "blueConstant",
            "Blue Constant",
            String(constant)).succeeded());
    const auto compiled = GraphCompiler().compile(loaded.graph);
    REQUIRE(compiled.succeeded());

    constexpr int blockSize = 256;
    constexpr int sampleCount = 2048;
    constexpr double sampleRate = 44'100.0;
    constexpr double blockDuration = blockSize / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = blockSize;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    renderer.setRandomSeedForTesting(0x42564c55);
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 60, velocity),
            MidiEventSource::PerformanceKeyboard,
            1.0));

    AudioBuffer<float> output(2, blockSize);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    std::vector<float> samples;
    samples.reserve(sampleCount);
    double callbackTime = 1.0;
    for (int start = 0; start < sampleCount; start += blockSize) {
        renderer.process(
                queue,
                channels,
                2,
                blockSize,
                sampleRate,
                callbackTime);
        samples.insert(
                samples.end(),
                output.getReadPointer(0),
                output.getReadPointer(0) + blockSize);
        callbackTime += blockDuration;
    }
    return samples;
#else
    ignoreUnused(source, constant, velocity);
    return {};
#endif
}

double sinusoidMagnitude(
        const std::vector<float>& samples,
        double sampleRate,
        double frequency) {
    constexpr size_t start = 4096;
    constexpr size_t count = 4096;
    REQUIRE(samples.size() >= start + count);
    double real {};
    double imaginary {};
    for (size_t index = 0; index < count; ++index) {
        const double phase = MathConstants<double>::twoPi
                * frequency
                * (double) index
                / sampleRate;
        const double window = 0.5 - 0.5 * std::cos(
                MathConstants<double>::twoPi * (double) index / (double) (count - 1));
        const double sample = (double) samples[start + index] * window;
        real += sample * std::cos(phase);
        imaginary -= sample * std::sin(phase);
    }
    return std::sqrt(real * real + imaginary * imaginary);
}

double lagCorrelation(const std::vector<float>& samples, size_t lag) {
    constexpr size_t start = 2048;
    constexpr size_t count = 4096;
    REQUIRE(samples.size() >= start + count + lag);
    double product {};
    double firstEnergy {};
    double secondEnergy {};
    for (size_t index = 0; index < count; ++index) {
        const double first = samples[start + index];
        const double second = samples[start + index + lag];
        product += first * second;
        firstEnergy += first * first;
        secondEnergy += second * second;
    }
    return product / std::sqrt(firstEnergy * secondEnergy);
}

}

TEST_CASE("Realtime audio telemetry does not change rendered output",
        "[cycle-v2][audio-device][realtime][performance]") {
    const auto render = [](bool instrumented) {
        const auto compiled = GraphCompiler().compile(NodeGraph::createDemoGraph());
        REQUIRE(compiled.succeeded());
        AudioExecutionSpec spec;
        spec.maximumFrameCount = 256;
        auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 23, spec);
        RealtimeGraphRenderer renderer;
        RealtimeMidiEventQueue queue;
        renderer.setPreparedGraph(prepared.get());
        renderer.setRandomSeedForTesting(0x41554449);
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 60, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                1.0));
        AudioBuffer<float> output(2, 256);
        float* channels[] {
                output.getWritePointer(0),
                output.getWritePointer(1)
            };
        AudioPerformanceMetrics::RealtimeSample sample;
        renderer.process(
                queue,
                channels,
                2,
                256,
                44'100.0,
                1.0,
                instrumented ? &sample : nullptr);
        if (instrumented) {
            REQUIRE(sample.timeSources.waveform.waveformSegments > 0);
            REQUIRE(sample.timeSources.waveform.integralSegments == 0);
            REQUIRE(sample.graphRevision == 23);
            REQUIRE(sample.executionStepCount == compiled.plan.steps.size());
            REQUIRE(sample.activeVoiceCount == 1);
            REQUIRE(sample.oscillatorRegionRenderCount > 0);
            REQUIRE(sample.oscillatorRecipeRenderCount > 0);
            REQUIRE(sample.oscillatorMixedLaneCount > 0);
            REQUIRE(sample.oscillatorStageDurations[(size_t)
                    AudioPerformanceMetrics::OscillatorStage::RegionRendering] > 0);
            REQUIRE(sample.oscillatorRecipeStageOperationCounts[(size_t)
                    OscillatorRecipeStage::TimeSourceRendering] > 0);
            for (const auto stage : {
                    CycleDsp::SourceRenderStage::MorphResolution,
                    CycleDsp::SourceRenderStage::Rasterization,
                    CycleDsp::SourceRenderStage::Sampling,
                    CycleDsp::SourceRenderStage::StereoCopy }) {
                REQUIRE(sample.timeSources.operations[(size_t) stage] > 0);
            }
        }
        return std::vector<float>(
                output.getReadPointer(0),
                output.getReadPointer(0) + output.getNumSamples());
    };

    const auto plain = render(false);
    const auto instrumented = render(true);
    REQUIRE(Buffer<float>(
            const_cast<float*>(plain.data()),
            (int) plain.size()).normDiffL2({
                const_cast<float*>(instrumented.data()),
                (int) instrumented.size()
            }) == 0.f);
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

TEST_CASE("Realtime keyboard velocity follows the attached Mod Triple mapping",
        "[cycle-v2][audio-device][realtime][modulation][velocity]") {
    constexpr uint8 velocity = 32;
    const auto mappedVelocity = renderAttachedVelocityMapping("velocity", 0.f, velocity);
    const auto inverseVelocity = renderAttachedVelocityMapping("inverseVelocity", 0.f, velocity);
    const auto constant = renderAttachedVelocityMapping("constant", 0.f, velocity);

    REQUIRE(Buffer<float>(
            const_cast<float*>(mappedVelocity.data()),
            (int) mappedVelocity.size()).normDiffL2({
                const_cast<float*>(inverseVelocity.data()),
                (int) inverseVelocity.size()
            }) > 0.01f);
    REQUIRE(Buffer<float>(
            const_cast<float*>(mappedVelocity.data()),
            (int) mappedVelocity.size()).normDiffL2({
                const_cast<float*>(constant.data()),
                (int) constant.size()
            }) > 0.01f);
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

TEST_CASE("Astral realtime pitch follows MIDI rather than the audio callback period",
        "[cycle-v2][audio-device][realtime][midi][astral][parity]") {
#if defined(CYCLE_V2_SOURCE_DIR)
    constexpr double sampleRate = 44'100.0;
    constexpr double callbackFrequency = sampleRate / 512.0;
    const std::vector<float> midi60 = renderAstralRealtimeNote(60);
    const std::vector<float> midi72 = renderAstralRealtimeNote(72);
    const double midi60Fundamental = sinusoidMagnitude(midi60, sampleRate, 261.625565);
    const double midi72Fundamental = sinusoidMagnitude(midi72, sampleRate, 523.251131);
    const double midi60CallbackTone = sinusoidMagnitude(
            midi60,
            sampleRate,
            callbackFrequency);
    const double midi72CallbackTone = sinusoidMagnitude(
            midi72,
            sampleRate,
            callbackFrequency);

    REQUIRE(midi60Fundamental > midi60CallbackTone * 100.0);
    REQUIRE(midi72Fundamental > midi72CallbackTone * 100.0);
    REQUIRE(lagCorrelation(midi60, 169) > lagCorrelation(midi60, 512));
    REQUIRE(lagCorrelation(midi72, 84) > lagCorrelation(midi72, 512));
#else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
#endif
}

TEST_CASE("Realtime renderer supplies the mixed voice terminal through Global Input",
        "[cycle-v2][audio-device][realtime][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "voiceTerminal", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "boundary", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "boundary", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({ "voiceTerminal", "out", "voiceOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    AudioExecutionSpec spec;
    spec.maximumFrameCount = 256;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 71, spec);
    REQUIRE(prepared != nullptr);
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
    renderer.process(queue, channels, 2, 256, 44'100.0, 1.0);

    const auto diagnostics = renderer.diagnostics(queue);
    REQUIRE(diagnostics.peak > 0.f);
    REQUIRE(diagnostics.leftPeak > 0.f);
    REQUIRE(diagnostics.rightPeak > 0.f);
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

TEST_CASE("Realtime graph replacement preserves channel controllers for the next note",
        "[cycle-v2][audio-device][realtime][modulation][graph-adoption]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "mod", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({
            "mod", "value", "out", "time",
            PortDomain::ControlSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    const auto renderAfterReplacement = [&](int controllerValue) {
        constexpr int frameCount = 64;
        constexpr double sampleRate = 44'100.0;
        constexpr double blockDuration = frameCount / sampleRate;
        AudioExecutionSpec spec;
        spec.maximumFrameCount = frameCount;
        spec.sampleRate = sampleRate;
        auto initial = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
        auto replacement = RealtimeGraphRenderer::prepareGraph(compiled.plan, 2, spec);
        RealtimeGraphRenderer renderer;
        RealtimeMidiEventQueue queue;
        renderer.setPreparedGraph(initial.get());
        REQUIRE(queue.enqueue(
                MidiMessage::controllerEvent(1, 1, controllerValue),
                MidiEventSource::PerformanceKeyboard,
                1.0));

        AudioBuffer<float> output(2, frameCount);
        float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
        renderer.process(queue, channels, 2, frameCount, sampleRate, 1.0);
        renderer.setPreparedGraph(replacement.get());
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 60, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                1.0 + blockDuration));
        renderer.process(
                queue,
                channels,
                2,
                frameCount,
                sampleRate,
                1.0 + blockDuration);
        REQUIRE(renderer.diagnostics(queue).graphRevision == 2);
        return output.getSample(0, frameCount - 1);
    };

    REQUIRE(renderAfterReplacement(127) > renderAfterReplacement(0) + 0.1f);
}

TEST_CASE("Realtime voice-time clock uses the compiled Voice Context length",
        "[cycle-v2][audio-device][realtime][voice-context][voice-length]") {
    const auto renderDelta = [](float voiceLength) {
        GraphNodeFactory factory;
        GraphEditor editor;
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::ModulationSource, "time", {}));
        graph.replaceNodeParameters("time", {
                { "source", "Source", "voiceTime" },
                { "controller", "Controller", "1" },
                { "constant", "Constant", "0.5" }
        });
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        REQUIRE(GraphNodeStateEditor().setNodeParameter(
                graph,
                "voice",
                "voiceLength",
                "Voice Length",
                String(voiceLength)).succeeded());
        REQUIRE(editor.connect(
                graph,
                { "voice", "context", false },
                { "mesh", "context", true }).succeeded());
        REQUIRE(editor.connect(
                graph,
                { "time", "value", false },
                { "out", "time", true }).succeeded());
        const auto compiled = GraphCompiler().compile(graph);
        REQUIRE(compiled.succeeded());
        REQUIRE(compiled.plan.voiceContexts.front().voiceDurationSeconds
                == Catch::Approx(CycleDsp::voiceLengthSeconds(voiceLength)));

        constexpr int frameCount = 10;
        constexpr double sampleRate = 1'000.0;
        AudioExecutionSpec spec;
        spec.maximumFrameCount = frameCount;
        spec.sampleRate = sampleRate;
        auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 31, spec);
        RealtimeGraphRenderer renderer;
        RealtimeMidiEventQueue queue;
        renderer.setPreparedGraph(prepared.get());
        REQUIRE(queue.enqueue(
                MidiMessage::noteOn(1, 60, (uint8) 100),
                MidiEventSource::PerformanceKeyboard,
                1.0));
        AudioBuffer<float> output(2, frameCount);
        float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
        renderer.process(queue, channels, 2, frameCount, sampleRate, 1.0);
        return output.getSample(0, frameCount - 1) - output.getSample(0, 0);
    };

    const float oneSecond = renderDelta(CycleDsp::voiceLengthUnitValue(1.0));
    const float oneTenthSecond = renderDelta(CycleDsp::voiceLengthUnitValue(0.1));
    REQUIRE(oneSecond > 0.f);
    REQUIRE(oneTenthSecond == Catch::Approx(10.f * oneSecond).margin(1.0e-6f));
}

TEST_CASE("Realtime graph renderer stops immediately without a volume envelope",
        "[cycle-v2][audio-device][realtime][midi][release]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeNode("env");
    graph.removeNode("multiply");
    graph.addEdge({
            "ifft", "time", "voiceOutput", "time",
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
    graph.addEdge({
            "ifft", "time", "voiceOutput", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
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
    const auto globalOutputEdge = std::find_if(
            graph.getEdges().begin(),
            graph.getEdges().end(),
            [](const Edge& edge) {
                return edge.sourceNodeId == "globalInput"
                        && edge.destNodeId == "out";
            });
    REQUIRE(globalOutputEdge != graph.getEdges().end());
    graph.removeEdgeAt((size_t) std::distance(
            graph.getEdges().begin(),
            globalOutputEdge));
    graph.addEdge({
            "globalInput", "time", "delay", "time",
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

TEST_CASE("Compiled linked-stereo graph preserves distinct channels through Delay",
        "[cycle-v2][audio-device][realtime][delay][stereo]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("Icycle.cyclegraph");
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(
            preset.loadFileAsString());
    REQUIRE(loaded.succeeded());
    NodeGraph graph = loaded.graph;
    const auto delayInput = std::find_if(
            graph.getEdges().begin(),
            graph.getEdges().end(),
            [](const Edge& edge) {
                return edge.sourceNodeId == "impulseResponse"
                        && edge.destNodeId == "delay";
            });
    REQUIRE(delayInput != graph.getEdges().end());
    REQUIRE(GraphEditor().removeEdgeAt(
            graph,
            (size_t) std::distance(graph.getEdges().begin(), delayInput)).succeeded());

    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::StereoSplit,
            "delayStereoSplit",
            {}));
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::StereoJoin,
            "delayStereoJoin",
            {}));
    graph.addEdge({
            "impulseResponse", "time", "delayStereoSplit", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delayStereoSplit", "left", "delayStereoJoin", "left",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delayStereoSplit", "right", "delayStereoJoin", "right",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delayStereoJoin", "time", "delay", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    REQUIRE(GraphValidator().isValid(graph));

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    constexpr int frameCount = 256;
    constexpr double sampleRate = 44'100.0;
    constexpr double blockDuration = frameCount / sampleRate;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    spec.sampleRate = sampleRate;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 53, spec);
    RealtimeGraphRenderer renderer;
    RealtimeMidiEventQueue queue;
    renderer.setPreparedGraph(prepared.get());
    REQUIRE(queue.enqueue(
            MidiMessage::noteOn(1, 48, (uint8) 100),
            MidiEventSource::PerformanceKeyboard,
            1.0));

    AudioBuffer<float> output(2, frameCount);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    float stereoDifference = 0.f;
    double callbackTime = 1.0;
    for (int block = 0; block < 8; ++block) {
        renderer.process(
                queue,
                channels,
                2,
                frameCount,
                sampleRate,
                callbackTime);
        for (int sample = 0; sample < frameCount; ++sample) {
            stereoDifference = jmax(
                    stereoDifference,
                    std::abs(output.getSample(0, sample) - output.getSample(1, sample)));
        }
        callbackTime += blockDuration;
    }

    REQUIRE(renderer.diagnostics(queue).leftPeak > 0.f);
    REQUIRE(renderer.diagnostics(queue).rightPeak > 0.f);
    REQUIRE(stereoDifference > 1.0e-4f);
  #endif
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
    AudioPerformanceMetrics::RealtimeSample firstPerformance;
    renderer.process(
            queue,
            channels,
            2,
            64,
            44100.0,
            1.0,
            &firstPerformance);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 0);
    REQUIRE(firstPerformance.dequeuedMidiEventCount == 1);
    REQUIRE(firstPerformance.sortedMidiItemCount == 1);

    AudioPerformanceMetrics::RealtimeSample secondPerformance;
    renderer.process(
            queue,
            channels,
            2,
            64,
            44100.0,
            2.0,
            &secondPerformance);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);
    REQUIRE(renderer.diagnostics(queue).peak > 0.f);
    REQUIRE(secondPerformance.dequeuedMidiEventCount == 0);
    REQUIRE(secondPerformance.sortedMidiItemCount == 0);
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
