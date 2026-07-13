#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Runtime/GraphAudioExecutor.h"

#include <algorithm>

using namespace CycleV2;

namespace {

const NodeAudioResult& findNodeAudio(const GraphAudioResult& result, const String& nodeId) {
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [&](const NodeAudioResult& node) {
                return node.nodeId == nodeId;
            });

    REQUIRE(found != result.nodes.end());
    return *found;
}

const std::vector<float>& samples(const SignalPayload& payload) {
    return payload.block.samples;
}

}

TEST_CASE("Graph audio executor renders source through envelope multiply to output", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 240.f, 180.f }));
    graph.addNode(factory.createNode(NodeKind::Multiply, "mul", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 760.f, 0.f }));

    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "wave", "out", "mul", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "mul", "right", PortDomain::EnvelopeSignal, false });
    graph.addEdge({ "mul", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioProcessTiming timing;
    timing.sampleRate = 8.0;
    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 5, timing, voice);
    const auto& wave = samples(findNodeAudio(result, "wave").output);
    const auto& envelope = samples(findNodeAudio(result, "env").output);

    REQUIRE(envelope.front() < 0.001f);
    REQUIRE(envelope.back() > envelope.front());
    REQUIRE(samples(result.output)[3] == Catch::Approx(wave[3] * envelope[3]));
    REQUIRE(result.output.traversalGrid.isValid());
    REQUIRE(result.output.traversalGrid.columns == 8);
    REQUIRE(result.output.traversalGrid.rows == 5);
    REQUIRE(wave == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(findNodeAudio(result, "wave").output.traversalGrid.isValid());
    REQUIRE(samples(findNodeAudio(result, "mul").output) == samples(result.output));
    REQUIRE(findNodeAudio(result, "mul").output.traversalGrid.values
            != findNodeAudio(result, "wave").output.traversalGrid.values);
}

TEST_CASE("Graph audio executor applies envelope vectors along time rows", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 0.f, 180.f }));
    graph.addNode(factory.createNode(NodeKind::Multiply, "mul", { 260.f, 0.f }));

    graph.addEdge({ "wave", "out", "mul", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "mul", "right", PortDomain::EnvelopeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const auto result = GraphAudioExecutor().process(
            graph,
            compileResult.plan,
            64,
            {},
            voice);
    const auto& wave = findNodeAudio(result, "wave").output;
    const auto& env = findNodeAudio(result, "env").output;
    const auto& multiplied = findNodeAudio(result, "mul").output;

    REQUIRE(wave.traversalGrid.isValid());
    REQUIRE(env.traversalGrid.isValid());
    REQUIRE(multiplied.traversalGrid.isValid());
    REQUIRE(wave.traversalGrid.columns == multiplied.traversalGrid.columns);
    REQUIRE(wave.traversalGrid.rows == multiplied.traversalGrid.rows);
    REQUIRE(env.traversalGrid.rows == wave.traversalGrid.rows);

    float maxError = 0.f;
    for (size_t column = 0; column < multiplied.traversalGrid.columns; ++column) {
        for (size_t row = 0; row < multiplied.traversalGrid.rows; ++row) {
            const size_t index = column * multiplied.traversalGrid.rows + row;
            const float expected = wave.traversalGrid.values[index] * env.traversalGrid.values[row];
            maxError = std::max(maxError, std::abs(multiplied.traversalGrid.values[index] - expected));
        }
    }

    REQUIRE(maxError < 1.0e-5f);
}

TEST_CASE("Graph audio node payloads expose transformed grids for spy taps", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 780.f, 0.f }));

    auto mesh = factory.createNode(NodeKind::TrilinearMesh, "operand", { 260.f, 200.f });
    mesh.parameters = {
            { "yellow", "Yellow", "0.75" },
            { "red", "Red", "0.5" },
            { "blue", "Blue", "0.5" },
            { "primaryAxis", "Primary Axis", "yellow" }
    };
    graph.addNode(mesh);

    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "operand", "out", "add", "right", PortDomain::ControlSignal, false });
    graph.addEdge({ "add", "out", "shape", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "shape", "time", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 8);
    const auto& wave = findNodeAudio(result, "wave").output;
    const auto& add = findNodeAudio(result, "add").output;
    const auto& shape = findNodeAudio(result, "shape").output;

    REQUIRE(wave.traversalGrid.isValid());
    REQUIRE(add.traversalGrid.isValid());
    REQUIRE(shape.traversalGrid.isValid());
    REQUIRE(add.traversalGrid.values != wave.traversalGrid.values);
    REQUIRE(shape.traversalGrid.values != add.traversalGrid.values);
    REQUIRE(result.output.traversalGrid.values == shape.traversalGrid.values);
}

TEST_CASE("Graph audio executor passes parameters to node processors", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.getNodesForEditing().back().parameters = { { "level", "Level", "0.5" } };
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 260.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 3);

    REQUIRE(samples(result.output) == std::vector<float> { 0.f, 0.25f, 0.5f });
}

TEST_CASE("Graph audio executor preserves per-node processor state between blocks", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", { 260.f, 0.f }));
    graph.getNodesForEditing().back().parameters = {
            { "enabled", "Enabled", "1" },
            { "time", "Time", "0" },
            { "feedback", "Feedback", "1" },
            { "wet", "Wet", "1" }
    };
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 520.f, 0.f }));
    graph.addEdge({ "wave", "out", "delay", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "delay", "time", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    AudioProcessTiming timing;
    timing.sampleRate = 128.0;
    const auto first = executor.process(graph, compileResult.plan, 8, timing);
    const auto second = executor.process(graph, compileResult.plan, 8, timing);
    const auto fresh = GraphAudioExecutor().process(graph, compileResult.plan, 8, timing);

    REQUIRE(samples(first.output) == samples(fresh.output));
    REQUIRE(samples(second.output) != samples(fresh.output));
}

TEST_CASE("Graph audio executor keeps envelope state independent per voice", "[cycle-v2][runtime][envelope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 0.f, 0.f }));

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    AudioProcessTiming timing;
    timing.sampleRate = 16.0;
    AudioVoiceContext firstVoice;
    firstVoice.voiceIndex = 0;
    firstVoice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioVoiceContext secondVoice;
    secondVoice.voiceIndex = 1;
    secondVoice.events.push_back({ NoteLifecycleType::NoteOn, 0, 1 });

    const auto firstStart = executor.process(graph, compileResult.plan, 4, timing, firstVoice);
    const auto firstContinued = executor.process(
            graph,
            compileResult.plan,
            4,
            timing,
            AudioVoiceContext { 0, {} });
    const auto secondStart = executor.process(graph, compileResult.plan, 4, timing, secondVoice);

    const auto& firstStartEnvelope = samples(findNodeAudio(firstStart, "env").output);
    const auto& firstContinuedEnvelope = samples(findNodeAudio(firstContinued, "env").output);
    const auto& secondStartEnvelope = samples(findNodeAudio(secondStart, "env").output);
    REQUIRE(secondStartEnvelope == firstStartEnvelope);
    REQUIRE(firstContinuedEnvelope != secondStartEnvelope);
}

TEST_CASE("Graph audio executor returns silence for disconnected output", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Output, "out", { 0.f, 0.f }));

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 3);

    REQUIRE(samples(result.output) == std::vector<float> { 0.f, 0.f, 0.f });
}

TEST_CASE("Graph audio executor routes multi-output node buffers by port", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 780.f, 0.f }));
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, false });
    graph.addEdge({ "ifft", "time", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioProcessTiming timing;
    timing.sampleRate = 8.0;
    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4, timing, voice);
    const auto& fft = findNodeAudio(result, "fft");

    REQUIRE(fft.outputs.size() == 2);
    REQUIRE(fft.outputs[0].first == "mag");
    REQUIRE(fft.outputs[0].second.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(fft.outputs[0].second.traversalGrid.isValid());
    REQUIRE(fft.outputs[1].first == "phase");
    REQUIRE(fft.outputs[1].second.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(fft.outputs[1].second.traversalGrid.isValid());
    REQUIRE(samples(result.output)[0] == Catch::Approx(0.f).margin(1.0e-5f));
    REQUIRE(samples(result.output)[1] == Catch::Approx(1.f / 3.f).margin(1.0e-5f));
    REQUIRE(samples(result.output)[2] == Catch::Approx(2.f / 3.f).margin(1.0e-5f));
    REQUIRE(samples(result.output)[3] == Catch::Approx(1.f).margin(1.0e-5f));
}

TEST_CASE("Graph audio executor renders the demo graph through resolved mesh operands", "[cycle-v2][runtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioProcessTiming timing;
    timing.sampleRate = 8.0;
    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4, timing, voice);

    REQUIRE(findNodeAudio(result, "waveMesh").output.domain == PortDomain::TimeSignal);
    REQUIRE(findNodeAudio(result, "magMesh").output.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findNodeAudio(result, "phaseMesh").output.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findNodeAudio(result, "fft").outputs.size() == 2);
    REQUIRE(result.output.domain == PortDomain::TimeSignal);
    REQUIRE(samples(result.output).size() == 4);
    REQUIRE(result.output.traversalGrid.isValid());
    const auto& envelope = samples(findNodeAudio(result, "env").output);
    REQUIRE(envelope.back() > envelope.front());
}
