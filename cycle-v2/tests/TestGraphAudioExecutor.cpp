#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Runtime/GraphAudioExecutor.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>

namespace {

thread_local bool countRealtimeAllocations = false;
std::atomic<size_t> realtimeAllocationCount {};

}

void* operator new(std::size_t size) {
    if (countRealtimeAllocations) {
        ++realtimeAllocationCount;
    }
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

using namespace CycleV2;

namespace {

class ScopedRealtimeAllocationCount {
public:
    ScopedRealtimeAllocationCount() {
        realtimeAllocationCount = 0;
        countRealtimeAllocations = true;
    }

    ~ScopedRealtimeAllocationCount() {
        countRealtimeAllocations = false;
    }

    size_t count() const { return realtimeAllocationCount.load(); }
};

class FanOutObserver final : public GraphProcessObserver {
public:
    void nodeProcessed(const String& nodeId, const AudioProcessContext& context) override {
        if (nodeId == "wave" && !context.outputs.empty()) {
            sourceSamples = context.outputs.front().block.samples.data();
        }
        if (nodeId == "add" && !context.inputViews.empty()
                && context.inputViews.front() != nullptr) {
            consumerSamples = context.inputViews.front()->block.samples.data();
        }
        ++observedNodes;
    }

    const float* sourceSamples {};
    const float* consumerSamples {};
    size_t observedNodes {};
};

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

TEST_CASE("Graph control edges drive absolute Envelope morph without graph edits",
        "[cycle-v2][runtime][envelope][modulation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ImageSource, "redControl", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({
            "redControl", "out", "env", "red", PortDomain::ControlSignal, false
    });
    graph.addEdge({ "wave", "out", "multiply", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, false });
    graph.addEdge({ "multiply", "out", "output", "time", PortDomain::TimeSignal, false });
    Node* envelope = graph.findNodeForEditing("env");
    REQUIRE(envelope != nullptr);
    for (auto& parameter : envelope->parameters) {
        if (parameter.id == "dynamic") {
            parameter.value = "1";
        }
    }

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphAudioExecutor executor;
    AudioVoiceContext noteOn;
    noteOn.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const auto initial = executor.process(graph, compiled.plan, 16, {}, noteOn);
    const auto initialGrid = findNodeAudio(initial, "env").output.traversalGrid.values;

    REQUIRE(executor.serviceNonRealtimePreparation() == 1);
    const auto adopted = executor.process(graph, compiled.plan, 16, {}, {});
    REQUIRE(findNodeAudio(adopted, "env").output.traversalGrid.values != initialGrid);
    REQUIRE(executor.serviceNonRealtimePreparation() == 0);
    REQUIRE(parameterValueForNode(*graph.findNode("env"), "red") == "0.5");
}

TEST_CASE("Graph audio executor applies envelope phase across traversal columns", "[cycle-v2][runtime]") {
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
            const size_t envelopeColumn = column * env.traversalGrid.columns
                    / multiplied.traversalGrid.columns;
            const float expected = wave.traversalGrid.values[index]
                    * env.traversalGrid.values[envelopeColumn * env.traversalGrid.rows];
            maxError = std::max(maxError, std::abs(multiplied.traversalGrid.values[index] - expected));
        }
    }

    REQUIRE(maxError < 1.0e-5f);
}

TEST_CASE("Published curve edits change their node and downstream graph output",
        "[contract][cycle-v2][invalidation][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "shape", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "shape", "time", "multiply", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, false });
    graph.addEdge({ "multiply", "out", "out", "time", PortDomain::TimeSignal, false });

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioProcessTiming timing;
    timing.sampleRate = 16.0;

    const auto initialPlan = GraphCompiler().compile(graph);
    REQUIRE(initialPlan.succeeded());
    const auto initial = GraphAudioExecutor().process(graph, initialPlan.plan, 16, timing, voice);
    const auto initialShape = samples(findNodeAudio(initial, "shape").output);
    const auto initialEnvelope = samples(findNodeAudio(initial, "env").output);
    const auto initialOutput = samples(initial.output);

    FlatCurveModel shapeModel;
    REQUIRE(shapeModel.loadSnapshot(parameterValueForNode(
            *graph.findNode("shape"), CurveNodeModelCodec::snapshotParameterId())));
    auto flatVertices = shapeModel.getVertices();
    for (auto& vertex : flatVertices) {
        vertex.y = 0.25f;
    }
    REQUIRE(shapeModel.replaceVertices(std::move(flatVertices)));
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "shape",
            CurveNodeModelCodec::snapshotParameterId(),
            "Curve Model Snapshot",
            shapeModel.snapshot()).succeeded());

    const auto shapedPlan = GraphCompiler().compile(graph);
    REQUIRE(shapedPlan.succeeded());
    const auto shaped = GraphAudioExecutor().process(graph, shapedPlan.plan, 16, timing, voice);
    const auto shapedNode = samples(findNodeAudio(shaped, "shape").output);
    REQUIRE(shapedNode != initialShape);
    REQUIRE(samples(shaped.output) != initialOutput);
    REQUIRE(shapedNode[1] == Catch::Approx(1.f / 6.f).margin(1.0e-5f));
    for (size_t index = 2; index + 1 < shapedNode.size(); ++index) {
        REQUIRE(shapedNode[index] == Catch::Approx(shapedNode[1]).margin(1.0e-5f));
    }

    EnvelopeNodeModel envelopeModel;
    REQUIRE(envelopeModel.syncFromNode(*graph.findNode("env")));
    for (auto* vertex : envelopeModel.getMesh().getVerts()) {
        vertex->values[Vertex::Amp] = 0.25f;
    }
    REQUIRE(envelopeModel.synchronizeFromMesh(envelopeModel.getMesh().getCubes().front()));
    REQUIRE(GraphEditor().setNodeParameter(
            graph,
            "env",
            CurveNodeModelCodec::snapshotParameterId(),
            "Curve Model Snapshot",
            envelopeModel.snapshot()).succeeded());

    const auto envelopePlan = GraphCompiler().compile(graph);
    REQUIRE(envelopePlan.succeeded());
    const auto enveloped = GraphAudioExecutor().process(graph, envelopePlan.plan, 16, timing, voice);
    const auto editedEnvelope = samples(findNodeAudio(enveloped, "env").output);
    const auto editedOutput = samples(enveloped.output);
    REQUIRE(editedEnvelope != initialEnvelope);
    REQUIRE(editedOutput != samples(shaped.output));
    for (size_t index = 0; index < editedOutput.size(); ++index) {
        REQUIRE(editedEnvelope[index] == Catch::Approx(0.25f).margin(1.0e-4f));
        REQUIRE(editedOutput[index]
                == Catch::Approx(shapedNode[index] * editedEnvelope[index]).margin(1.0e-5f));
    }
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
    graph.replaceNodeParameters("wave", { { "level", "Level", "0.5" } });
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
    graph.replaceNodeParameters("delay", {
            { "enabled", "Enabled", "1" },
            { "time", "Time", "0" },
            { "feedback", "Feedback", "1" },
            { "wet", "Wet", "1" }
    });
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

TEST_CASE("Graph audio execution preparation retains unchanged configuration revisions", "[cycle-v2][runtime][configuration]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 0.f, 0.f }));
    GraphCompiler compiler;
    const auto first = compiler.compile(graph);
    REQUIRE(first.succeeded());

    GraphAudioExecutor executor;
    const AudioExecutionSpec spec { 64, 48000.0, ChannelLayout::LinkedStereo };
    executor.prepareExecution(first.plan, spec);
    executor.prepareExecution(first.plan, spec);
    REQUIRE(executor.preparationCount("shape") == 1);

    REQUIRE(GraphEditor().setNodeParameter(graph, "shape", "pre", "Pre", "0.75").succeeded());
    const auto changed = compiler.compile(graph);
    REQUIRE(changed.succeeded());
    executor.prepareExecution(changed.plan, spec);
    REQUIRE(executor.preparationCount("shape") == 2);
}

TEST_CASE("Graph audio execution preparation distinguishes keys from restarted revision sources", "[cycle-v2][runtime][configuration]") {
    GraphNodeFactory factory;
    NodeGraph firstGraph;
    firstGraph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 0.f, 0.f }));
    const auto first = GraphCompiler().compile(firstGraph);
    REQUIRE(first.succeeded());

    NodeGraph changedGraph = firstGraph;
    REQUIRE(GraphEditor().setNodeParameter(changedGraph, "shape", "pre", "Pre", "0.75").succeeded());
    const auto changed = GraphCompiler().compile(changedGraph);
    REQUIRE(changed.succeeded());
    REQUIRE(changed.plan.steps.front().configuration.revision
            == first.plan.steps.front().configuration.revision);
    REQUIRE(changed.plan.steps.front().configuration.key
            != first.plan.steps.front().configuration.key);

    GraphAudioExecutor executor;
    const AudioExecutionSpec spec { 64, 48000.0, ChannelLayout::LinkedStereo };
    executor.prepareExecution(first.plan, spec);
    executor.prepareExecution(changed.plan, spec);

    REQUIRE(executor.preparationCount("shape") == 2);
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

TEST_CASE("Prepared graph audio dispatch remains available for every voice", "[cycle-v2][runtime][realtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());

    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    executor.prepareExecution(compiled.plan, spec, 0);
    executor.prepareExecution(compiled.plan, spec, 1);

    AudioVoiceContext firstVoice;
    firstVoice.voiceIndex = 0;
    AudioVoiceContext secondVoice;
    secondVoice.voiceIndex = 1;

    REQUIRE(executor.processRealtime(graph, compiled.plan, 16, {}, firstVoice).isValid());
    REQUIRE(executor.processRealtime(graph, compiled.plan, 16, {}, secondVoice).isValid());
    REQUIRE(executor.processRealtime(graph, compiled.plan, 16, {}, firstVoice).isValid());
    REQUIRE(executor.preparationCount("wave", 0) == 1);
    REQUIRE(executor.preparationCount("wave", 1) == 1);
}

TEST_CASE("Graph plan replacement removes stale processor state", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph firstGraph;
    firstGraph.addNode(factory.createNode(NodeKind::WaveSource, "oldWave", {}));
    const auto first = GraphCompiler().compile(firstGraph);
    REQUIRE(first.succeeded());

    GraphAudioExecutor executor;
    REQUIRE_FALSE(executor.process(firstGraph, first.plan, 8).nodes.empty());
    REQUIRE(executor.preparationCount("oldWave") == 1);

    NodeGraph replacementGraph;
    replacementGraph.addNode(factory.createNode(NodeKind::ImageSource, "newImage", {}));
    const auto replacement = GraphCompiler().compile(replacementGraph);
    REQUIRE(replacement.succeeded());
    REQUIRE_FALSE(executor.process(replacementGraph, replacement.plan, 8).nodes.empty());

    REQUIRE(executor.preparationCount("oldWave") == 0);
    REQUIRE(executor.preparationCount("newImage") == 1);
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

TEST_CASE("Graph audio executor exposes a bounded realtime output view", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 4;
    executor.prepareExecution(compileResult.plan, spec);
    const auto first = executor.processRealtime(graph, compileResult.plan, 4, {}, voice);
    const auto second = executor.processRealtime(graph, compileResult.plan, 4, {}, voice);

    REQUIRE(first.isValid());
    REQUIRE(second.isValid());
    REQUIRE(second.payload->block.samples == std::vector<float> { 0.f, 1.f / 3.f, 2.f / 3.f, 1.f });
}

TEST_CASE("Prepared graph audio processing performs no heap allocations", "[cycle-v2][runtime][realtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    executor.prepareExecution(compileResult.plan, spec);
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    REQUIRE(executor.processRealtime(graph, compileResult.plan, 64, {}, voice).isValid());

    ScopedRealtimeAllocationCount allocations;
    const auto output = executor.processRealtime(graph, compileResult.plan, 64, {}, voice);
    const auto shorterOutput = executor.processRealtime(graph, compileResult.plan, 32, {}, voice);

    REQUIRE(output.isValid());
    REQUIRE(shorterOutput.isValid());
    REQUIRE(allocations.count() == 0);
}

TEST_CASE("Dynamic Envelope request and adoption remain allocation-free on the realtime path",
        "[cycle-v2][runtime][realtime][envelope][modulation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ImageSource, "redControl", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({
            "redControl", "out", "env", "red", PortDomain::ControlSignal, false
    });
    graph.addEdge({ "wave", "out", "multiply", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, false });
    graph.addEdge({ "multiply", "out", "output", "time", PortDomain::TimeSignal, false });
    Node* envelope = graph.findNodeForEditing("env");
    REQUIRE(envelope != nullptr);
    for (auto& parameter : envelope->parameters) {
        if (parameter.id == "dynamic") {
            parameter.value = "1";
        }
    }

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    executor.prepareExecution(compiled.plan, spec);
    AudioVoiceContext noteOn;
    noteOn.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    REQUIRE(executor.processRealtime(graph, compiled.plan, 64, {}, {}).isValid());

    {
        ScopedRealtimeAllocationCount allocations;
        REQUIRE(executor.processRealtime(graph, compiled.plan, 64, {}, noteOn).isValid());
        REQUIRE(allocations.count() == 0);
    }
    REQUIRE(executor.serviceNonRealtimePreparation() == 1);
    {
        ScopedRealtimeAllocationCount allocations;
        REQUIRE(executor.processRealtime(graph, compiled.plan, 64, {}, {}).isValid());
        REQUIRE(allocations.count() == 0);
    }
}

TEST_CASE(
        "Prepared Trimesh traversal rendering performs no heap allocations",
        "[cycle-v2][runtime][realtime][trimesh]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    const MorphPosition center(0.5f, 0.5f, 0.5f);
    TrimeshGridwiseDsp dsp;
    dsp.setCyclic(true);
    dsp.prepare(*mesh, center, Vertex::Time, 32, 32);
    std::vector<float> destination(32 * 32);

    ScopedRealtimeAllocationCount allocations;
    for (const size_t columns : { 8u, 16u, 32u }) {
        REQUIRE(dsp.renderColumnsInto(
                *mesh,
                center,
                Vertex::Time,
                columns,
                Buffer<float>(destination.data(), (int) (columns * 32))));
    }

    REQUIRE(allocations.count() == 0);
    REQUIRE(dsp.counters().sliceCount == 56);
    REQUIRE(dsp.counters().bakeCount == 56);
    mesh->destroy();
}

TEST_CASE("Realtime observation is optional and fan-out shares compiled slot storage", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "wave", "out", "add", "right", PortDomain::TimeSignal, false });
    graph.addEdge({ "add", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    executor.prepareExecution(compiled.plan, spec);
    FanOutObserver observer;
    AudioVoiceContext voice;

    REQUIRE(executor.processRealtime(graph, compiled.plan, 16, {}, voice, &observer).isValid());
    REQUIRE(observer.observedNodes == 3);
    REQUIRE(observer.sourceSamples != nullptr);
    REQUIRE(observer.consumerSamples == observer.sourceSamples);
}
