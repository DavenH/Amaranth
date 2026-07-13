#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Runtime/GraphAudioExecutor.h"

#include <algorithm>
#include <cmath>

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

const SignalPayload& outputForPort(const NodeAudioResult& node, const String& portId) {
    const auto found = std::find_if(
            node.outputs.begin(),
            node.outputs.end(),
            [&](const std::pair<String, SignalPayload>& output) {
                return output.first == portId;
            });

    REQUIRE(found != node.outputs.end());
    return found->second;
}

float absoluteSum(const std::vector<float>& values) {
    float sum = 0.f;

    for (const float value : values) {
        sum += value >= 0.f ? value : -value;
    }

    return sum;
}

float absoluteDifferenceSum(const std::vector<float>& left, const std::vector<float>& right) {
    REQUIRE(left.size() == right.size());

    float sum = 0.f;
    for (size_t i = 0; i < left.size(); ++i) {
        const float diff = left[i] - right[i];
        sum += diff >= 0.f ? diff : -diff;
    }

    return sum;
}

void requireGridDeltaEquals(
        const SignalTraversalGrid& after,
        const SignalTraversalGrid& before,
        const SignalTraversalGrid& delta) {
    REQUIRE(after.columns == before.columns);
    REQUIRE(after.rows == before.rows);
    REQUIRE(delta.columns == after.columns);
    REQUIRE(delta.rows == after.rows);

    float maxError = 0.f;
    for (size_t i = 0; i < after.columns * after.rows; ++i) {
        const float error = std::abs(after.values[i] - before.values[i] - delta.values[i]);
        maxError = std::max(maxError, error);
    }

    REQUIRE(maxError < 1.0e-5f);
}

void requireMagnitudeGridAddEquals(
        const SignalTraversalGrid& after,
        const SignalTraversalGrid& before,
        const SignalTraversalGrid& delta) {
    REQUIRE(after.columns == before.columns);
    REQUIRE(after.rows == before.rows);
    REQUIRE(delta.columns == after.columns);
    REQUIRE(delta.rows == after.rows);

    float maxError = 0.f;
    for (size_t i = 0; i < after.columns * after.rows; ++i) {
        const float expected = jmax(0.f, before.values[i] + delta.values[i]);
        const float error = std::abs(after.values[i] - expected);
        maxError = std::max(maxError, error);
    }

    REQUIRE(maxError < 1.0e-5f);
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

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 5);
    const auto& wave = findNodeAudio(result, "wave").output;
    const auto& multiplied = findNodeAudio(result, "mul").output;

    REQUIRE(result.output.traversalGrid.isValid());
    REQUIRE(result.output.traversalGrid.columns == 8);
    REQUIRE(result.output.traversalGrid.rows == 5);
    REQUIRE(samples(wave) == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(wave.traversalGrid.isValid());
    REQUIRE(samples(multiplied) == samples(result.output));
    REQUIRE(samples(multiplied) != samples(wave));
    REQUIRE(multiplied.traversalGrid.values != wave.traversalGrid.values);
    REQUIRE(result.output.block.samples.back() == Catch::Approx(0.f).margin(1.0e-5f));
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

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 64);
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

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4);
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

TEST_CASE("Graph audio executor produces concrete FFT traversal grids", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "magSpy", { 520.f, -120.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "phaseSpy", { 520.f, 120.f }));
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "magSpy", "in", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "phaseSpy", "in", PortDomain::SpectralPhaseSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 16);
    const auto& fft = findNodeAudio(result, "fft");
    const auto& magnitude = outputForPort(fft, "mag");
    const auto& phase = outputForPort(fft, "phase");
    const auto& magSpy = findNodeAudio(result, "magSpy").output;
    const auto& phaseSpy = findNodeAudio(result, "phaseSpy").output;

    REQUIRE(magnitude.traversalGrid.isValid());
    REQUIRE(phase.traversalGrid.isValid());
    REQUIRE(magnitude.traversalGrid.columns == 8);
    REQUIRE(phase.traversalGrid.columns == 8);
    REQUIRE(magnitude.traversalGrid.rows == 9);
    REQUIRE(phase.traversalGrid.rows == 9);
    REQUIRE(magnitude.traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(phase.traversalGrid.metadata.valueDomain == PortDomain::SpectralPhaseSignal);
    REQUIRE(magnitude.traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(phase.traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(absoluteSum(magnitude.traversalGrid.values) > 0.01f);
    REQUIRE(absoluteSum(phase.traversalGrid.values) > 0.01f);
    REQUIRE(magSpy.traversalGrid.values == magnitude.traversalGrid.values);
    REQUIRE(phaseSpy.traversalGrid.values == phase.traversalGrid.values);
}

TEST_CASE("Graph audio executor transforms FFT traversal grids through frequency-domain math", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "addMag", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "beforeSpy", { 520.f, -140.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "afterSpy", { 780.f, 0.f }));

    auto operand = factory.createNode(NodeKind::TrilinearMesh, "operand", { 260.f, 180.f });
    operand.parameters = {
            { "yellow", "Yellow", "0.25" },
            { "red", "Red", "0.5" },
            { "blue", "Blue", "0.5" },
            { "primaryAxis", "Primary Axis", "yellow" }
    };
    graph.addNode(operand);

    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "beforeSpy", "in", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "mag", "addMag", "left", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "operand", "out", "addMag", "right", PortDomain::ControlSignal, false });
    graph.addEdge({ "addMag", "out", "afterSpy", "in", PortDomain::SpectralMagnitudeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto addStep = std::find_if(
            compileResult.plan.steps.begin(),
            compileResult.plan.steps.end(),
            [](const GraphExecutionStep& step) {
                return step.nodeId == "addMag";
            });
    REQUIRE(addStep != compileResult.plan.steps.end());
    REQUIRE(addStep->outputs.size() == 1);
    REQUIRE(addStep->inputs.size() == 2);
    INFO("add step output domain: " << static_cast<int>(addStep->outputs.front().domain));
    for (const auto& input : addStep->inputs) {
        INFO("add input " << input.sourceNodeId << "." << input.sourcePortId << " -> "
                          << input.destPortId << " domain " << static_cast<int>(input.domain));
    }

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 16);
    const auto& operandAudio = findNodeAudio(result, "operand").output;
    const auto& add = findNodeAudio(result, "addMag").output;
    const auto& before = findNodeAudio(result, "beforeSpy").output;
    const auto& after = findNodeAudio(result, "afterSpy").output;

    INFO("add domain: " << static_cast<int>(add.domain));
    INFO("add grid columns: " << add.traversalGrid.columns);
    INFO("add grid rows: " << add.traversalGrid.rows);
    REQUIRE(operandAudio.traversalGrid.isValid());
    REQUIRE(operandAudio.traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(operandAudio.traversalGrid.metadata.rowAxis == TraversalGridAxis::Frequency);
    REQUIRE(before.traversalGrid.isValid());
    REQUIRE(add.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(add.traversalGrid.isValid());
    REQUIRE(after.traversalGrid.isValid());
    REQUIRE(after.traversalGrid.metadata.valueDomain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(after.traversalGrid.values != before.traversalGrid.values);
    REQUIRE(absoluteSum(after.traversalGrid.values) > absoluteSum(before.traversalGrid.values));
    REQUIRE(*std::min_element(after.traversalGrid.values.begin(), after.traversalGrid.values.end()) >= 0.f);
    requireMagnitudeGridAddEquals(after.traversalGrid, before.traversalGrid, operandAudio.traversalGrid);
}

TEST_CASE("Graph audio executor rebuilds time traversal grids after FFT inverse", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Spy, "timeSpy", { 780.f, 0.f }));
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, false });
    graph.addEdge({ "ifft", "time", "timeSpy", "in", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 16);
    const auto& wave = findNodeAudio(result, "wave").output;
    const auto& ifft = findNodeAudio(result, "ifft").output;
    const auto& spy = findNodeAudio(result, "timeSpy").output;

    REQUIRE(wave.traversalGrid.isValid());
    REQUIRE(ifft.traversalGrid.isValid());
    REQUIRE(spy.traversalGrid.isValid());
    REQUIRE(ifft.traversalGrid.metadata.valueDomain == PortDomain::TimeSignal);
    REQUIRE(ifft.traversalGrid.metadata.rowAxis == TraversalGridAxis::Time);
    REQUIRE(ifft.traversalGrid.columns == wave.traversalGrid.columns);
    REQUIRE(ifft.traversalGrid.rows == wave.traversalGrid.rows);
    REQUIRE(absoluteSum(ifft.traversalGrid.values) > 0.01f);
    REQUIRE(spy.traversalGrid.values == ifft.traversalGrid.values);
}

TEST_CASE("Graph audio executor preserves traversal grids through FFT add passthrough IFFT", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "addMag", { 520.f, -80.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "addPhase", { 520.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 780.f, 0.f }));

    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "addMag", "left", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "addPhase", "left", PortDomain::SpectralPhaseSignal, false });
    graph.addEdge({ "addMag", "out", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "addPhase", "out", "ifft", "phase", PortDomain::SpectralPhaseSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 128);
    const auto& mesh = findNodeAudio(result, "mesh").output;
    const auto& fft = findNodeAudio(result, "fft");
    const auto& fftMag = outputForPort(fft, "mag");
    const auto& fftPhase = outputForPort(fft, "phase");
    const auto& addMag = findNodeAudio(result, "addMag").output;
    const auto& addPhase = findNodeAudio(result, "addPhase").output;
    const auto& ifft = findNodeAudio(result, "ifft").output;

    REQUIRE(mesh.traversalGrid.isValid());
    REQUIRE(fftMag.traversalGrid.isValid());
    REQUIRE(fftPhase.traversalGrid.isValid());
    REQUIRE(addMag.traversalGrid.isValid());
    REQUIRE(addPhase.traversalGrid.isValid());
    REQUIRE(ifft.traversalGrid.isValid());
    REQUIRE(absoluteDifferenceSum(addMag.traversalGrid.values, fftMag.traversalGrid.values) < 1.0e-5f);
    REQUIRE(absoluteDifferenceSum(addPhase.traversalGrid.values, fftPhase.traversalGrid.values) < 1.0e-5f);
    REQUIRE(ifft.traversalGrid.columns == mesh.traversalGrid.columns);
    REQUIRE(ifft.traversalGrid.rows == mesh.traversalGrid.rows);
    REQUIRE(absoluteDifferenceSum(ifft.traversalGrid.values, mesh.traversalGrid.values) < 1.0e-3f);
}

TEST_CASE("Graph audio executor renders the demo graph through resolved mesh operands", "[cycle-v2][runtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4);

    REQUIRE(findNodeAudio(result, "waveMesh").output.domain == PortDomain::TimeSignal);
    REQUIRE(findNodeAudio(result, "magMesh").output.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findNodeAudio(result, "phaseMesh").output.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findNodeAudio(result, "fft").outputs.size() == 2);
    REQUIRE(result.output.domain == PortDomain::TimeSignal);
    REQUIRE(samples(result.output).size() == 4);
    REQUIRE(result.output.traversalGrid.isValid());
    REQUIRE(samples(result.output) != std::vector<float> { 0.f, 0.f, 0.f, 0.f });
}
