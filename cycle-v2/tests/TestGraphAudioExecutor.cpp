#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Envelope/EnvelopePurpose.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshMeshState.h"
#include "../src/Runtime/GraphAudioExecutor.h"
#include "../src/Runtime/GraphPreviewExecutor.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>

#if JUCE_MAC
#include <pthread.h>
#endif

using namespace CycleV2;

namespace {

thread_local bool countRealtimeAllocations = false;
std::atomic<size_t> realtimeAllocationCount {};
#if JUCE_MAC
thread_local bool countRealtimeLocks = false;
std::atomic<size_t> realtimeLockCount {};
#endif

void setEnvelopePurpose(NodeGraph& graph, const String& nodeId, EnvelopePurpose purpose) {
    Node* node = graph.findNodeForEditing(nodeId);
    REQUIRE(node != nullptr);
    for (auto& parameter : node->parameters) {
        if (parameter.id == "purpose") {
            parameter.value = envelopePurposeToString(purpose);
        }
    }
    applyEnvelopePurpose(*node);
}

}

#if JUCE_MAC
extern "C" int countedPthreadMutexLock(pthread_mutex_t* mutex) {
    if (countRealtimeLocks) {
        ++realtimeLockCount;
    }
    return pthread_mutex_lock(mutex);
}

__attribute__((used)) static const struct {
    const void* replacement;
    const void* replacee;
} pthreadMutexLockInterpose
        __attribute__((section("__DATA,__interpose"))) = {
                (const void*) countedPthreadMutexLock,
                (const void*) pthread_mutex_lock
        };
#endif

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

class ScopedRealtimeLockCount {
public:
    ScopedRealtimeLockCount() {
#if JUCE_MAC
        realtimeLockCount = 0;
        countRealtimeLocks = true;
#endif
    }

    ~ScopedRealtimeLockCount() {
#if JUCE_MAC
        countRealtimeLocks = false;
#endif
    }

    size_t count() const {
#if JUCE_MAC
        return realtimeLockCount.load();
#else
        return 0;
#endif
    }
};

class FanOutObserver final : public GraphProcessObserver {
public:
    void nodeProcessed(const String& nodeId, const AudioProcessContext& context) override {
        preparedCollections = context.inputViews.isPrepared()
                && context.attachments.isPrepared()
                && context.outputPorts.isPrepared()
                && context.outputViews.isPrepared()
                && context.outputs.isPrepared();
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
    bool preparedCollections {};
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

const SignalBuffer& samples(const SignalPayload& payload) {
    return payload.block.samples;
}

const SignalPayload& outputForPort(const NodeAudioResult& result, const String& portId) {
    const auto found = std::find_if(
            result.outputs.begin(),
            result.outputs.end(),
            [&](const auto& output) {
                return output.first == portId;
            });

    REQUIRE(found != result.outputs.end());
    return found->second;
}

var sawtoothMeshTopology() {
    Mesh mesh("FftSawtooth");
    const auto addIntercept = [&mesh](float phase, float amplitude) {
        VertCube* cube = TrimeshMeshFactory::addVoiceCube(
                mesh,
                phase,
                phase,
                amplitude,
                amplitude,
                1.f);

        for (int index = 0; index < (int) VertCube::numVerts; ++index) {
            Vertex* vertex = cube->getVertex(index);
            vertex->values[Vertex::Phase] = phase;
            vertex->values[Vertex::Amp] = amplitude;
            vertex->values[Vertex::Curve] = 1.f;
        }
    };

    addIntercept(0.f, 0.f);
    addIntercept(0.999f, 1.f);
    addIntercept(1.f, 0.f);

    const var topology = mesh.writeJSON();
    mesh.destroy();
    return topology;
}

}

TEST_CASE("Graph audio executor renders source through envelope multiply to output", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 240.f, 180.f }));
    setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
    graph.addNode(factory.createNode(NodeKind::Multiply, "mul", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 760.f, 0.f }));

    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, ConnectionKind::Signal });
    graph.addEdge({ "wave", "out", "mul", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "env", "env", "mul", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });
    graph.addEdge({ "mul", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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

TEST_CASE("Incremental graph audio reuses unaffected branch outputs", "[cycle-v2][runtime][causal]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "changed", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "unchanged", {}));

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    const auto first = executor.processIncremental(
            graph,
            compileResult.plan,
            16,
            { "changed", "unchanged" });
    REQUIRE(first.nodes.size() == 2);
    const auto* unchangedResult = first.nodes[1];
    const float* unchangedSamples = unchangedResult->output.block.samples.data();
    CHECK(executor.diagnosticProcessCount("changed") == 1);
    CHECK(executor.diagnosticProcessCount("unchanged") == 1);

    const auto second = executor.processIncremental(
            graph,
            compileResult.plan,
            16,
            { "changed" });
    REQUIRE(second.nodes.size() == 2);
    CHECK(second.nodes[1] == unchangedResult);
    CHECK(second.nodes[1]->output.block.samples.data() == unchangedSamples);
    CHECK(executor.diagnosticProcessCount("changed") == 2);
    CHECK(executor.diagnosticProcessCount("unchanged") == 1);
}

TEST_CASE("Incremental graph audio stops between obsolete dirty nodes",
        "[cycle-v2][runtime][causal][cancellation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "first", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "second", {}));
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    int checks {};
    const auto result = executor.processIncremental(
            graph,
            compileResult.plan,
            16,
            { "first", "second" },
            [&] { return checks++ == 0; });

    CHECK(result.cancelled);
    CHECK(executor.diagnosticProcessCount("first") == 1);
    CHECK(executor.diagnosticProcessCount("second") == 0);
}

TEST_CASE("Graph control edges drive absolute Envelope morph without graph edits",
        "[cycle-v2][runtime][envelope][modulation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "redControl", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({
            "redControl", "value", "env", "red", PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({ "wave", "out", "multiply", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });
    graph.addEdge({ "multiply", "out", "output", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
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

TEST_CASE("Logarithmic Envelope applies the Cycle 1 transform to audio and traversal grids",
        "[cycle-v2][runtime][envelope][logarithmic]") {
    struct RenderedEnvelope {
        GraphAudioResult audio;
        GraphPreviewResult preview;
    };
    const auto render = [](bool logarithmic) {
        GraphNodeFactory factory;
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
        setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
        Node* envelope = graph.findNodeForEditing("env");
        REQUIRE(envelope != nullptr);
        for (auto& parameter : envelope->parameters) {
            if (parameter.id == "logarithmic") {
                parameter.value = logarithmic ? "1" : "0";
            }
            if (parameter.id == "level") {
                parameter.value = "0.75";
            }
        }
        graph.addSignalProbe({
                "envelopeProbe",
                "env",
                "env",
                {},
                {},
                "Envelope",
                0.5f,
                0
        });

        const auto compiled = GraphCompiler().compile(graph);
        REQUIRE(compiled.succeeded());
        AudioVoiceContext voice;
        voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
        auto audio = GraphAudioExecutor().process(graph, compiled.plan, 32, {}, voice);
        auto preview = GraphPreviewExecutor().render(
                compiled.plan,
                audio,
                graph.getSignalProbes(),
                32);
        return RenderedEnvelope { std::move(audio), std::move(preview) };
    };

    const auto linear = render(false);
    const auto logarithmic = render(true);
    const auto& linearEnvelope = findNodeAudio(linear.audio, "env").output;
    const auto& logarithmicEnvelope = findNodeAudio(logarithmic.audio, "env").output;
    REQUIRE(linearEnvelope.traversalGrid.isValid());
    REQUIRE(logarithmicEnvelope.traversalGrid.isValid());

    std::vector<float> expectedAudio(
            linearEnvelope.block.samples.begin(),
            linearEnvelope.block.samples.end());
    Buffer<float> expectedAudioBuffer(expectedAudio.data(), (int) expectedAudio.size());
    expectedAudioBuffer.mul(1.f / 0.75f);
    Arithmetic::applyInvLogMapping(expectedAudioBuffer, 30.f);
    expectedAudioBuffer.mul(0.75f);

    std::vector<float> expectedGrid(
            linearEnvelope.traversalGrid.values.begin(),
            linearEnvelope.traversalGrid.values.end());
    Buffer<float> expectedGridBuffer(expectedGrid.data(), (int) expectedGrid.size());
    expectedGridBuffer.mul(1.f / 0.75f);
    Arithmetic::applyInvLogMapping(expectedGridBuffer, 30.f);
    expectedGridBuffer.mul(0.75f);

    const auto maximumError = [](const auto& actual, const auto& expected) {
        REQUIRE(actual.size() == expected.size());
        float result {};
        for (size_t index = 0; index < expected.size(); ++index) {
            result = std::max(result, std::abs(actual[index] - expected[index]));
        }
        return result;
    };
    REQUIRE(maximumError(logarithmicEnvelope.block.samples, expectedAudio) < 0.00001f);
    REQUIRE(maximumError(logarithmicEnvelope.traversalGrid.values, expectedGrid) < 0.00001f);
    REQUIRE(logarithmic.preview.nodes.size() == 1);
    REQUIRE(logarithmic.preview.nodes.front().primary == logarithmicEnvelope.traversalGrid.values);
    REQUIRE(logarithmic.preview.nodes.front().secondary == logarithmicEnvelope.block.samples);
    REQUIRE(logarithmic.preview.probes.size() == 1);
    REQUIRE(logarithmic.preview.probes.front().values == logarithmicEnvelope.traversalGrid.values);
    REQUIRE(logarithmic.preview.probes.front().values != linear.preview.probes.front().values);
}

TEST_CASE("Graph audio executor applies envelope phase across traversal columns", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 0.f, 180.f }));
    setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
    graph.addNode(factory.createNode(NodeKind::Multiply, "mul", { 260.f, 0.f }));

    graph.addEdge({ "wave", "out", "mul", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "env", "env", "mul", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });

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
    setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "shape", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "shape", "time", "multiply", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });
    graph.addEdge({ "multiply", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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
    const auto initialModel = std::dynamic_pointer_cast<const CurveNodeModelState>(
            graph.findNode("shape")->model);
    REQUIRE(initialModel != nullptr);
    REQUIRE(initialModel->flatCurve() != nullptr);
    REQUIRE(shapeModel.copyFrom(*initialModel->flatCurve()));
    auto flatVertices = shapeModel.getVertices();
    for (auto& vertex : flatVertices) {
        vertex.y = 0.25f;
    }
    REQUIRE(shapeModel.replaceVertices(std::move(flatVertices)));
    REQUIRE(GraphEditor().replaceNodeModel(
            graph,
            "shape",
            initialModel->revision(),
            CurveNodeModelState::copyOf(shapeModel, shapeModel.revision())).succeeded());

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
    const auto currentEnvelopeModel = graph.findNode("env")->model;
    REQUIRE(GraphEditor().replaceNodeModel(
            graph,
            "env",
            currentEnvelopeModel->revision(),
            CurveNodeModelState::copyOf(envelopeModel, envelopeModel.revision())).succeeded());

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

    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "operand", "out", "add", "right", PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "add", "out", "shape", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "shape", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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
    graph.addEdge({ "wave", "out", "delay", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "delay", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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

    REQUIRE(executor.processRealtime(compiled.plan, 16, {}, firstVoice).isValid());
    REQUIRE(executor.processRealtime(compiled.plan, 16, {}, secondVoice).isValid());
    REQUIRE(executor.processRealtime(compiled.plan, 16, {}, firstVoice).isValid());
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
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });
    graph.addEdge({ "ifft", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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

TEST_CASE("Trimesh sawtooth survives an FFT and IFFT graph round trip",
        "[cycle-v2][runtime][fft][trimesh]") {
    constexpr size_t frameCount = 128;
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "saw", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 560.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 860.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 1160.f, 0.f }));
    graph.replaceNodeParameters("saw", {
            { "yellow", "Yellow", "0.5" },
            { "red", "Red", "0.5" },
            { "blue", "Blue", "0.5" },
            { "primaryAxis", "Primary Axis", "yellow" }
    });
    Mesh sawtoothMesh;
    REQUIRE(sawtoothMesh.readJSON(sawtoothMeshTopology()));
    graph.replaceNodeModel("saw", TrimeshNodeModelState::copyOf(sawtoothMesh, 2));
    sawtoothMesh.destroy();
    graph.addEdge({ "voice", "context", "saw", "context", PortDomain::DomainContext, ConnectionKind::Signal });
    graph.addEdge({ "saw", "out", "fft", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });
    graph.addEdge({ "ifft", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addSignalProbe({ "sawProbe", "saw", "out", "fft", "time", "Sawtooth", 0.5f, 0 });
    graph.addSignalProbe({ "magnitudeProbe", "fft", "mag", "ifft", "mag", "Magnitude 1/n", 0.5f, 1 });
    graph.addSignalProbe({ "roundTripProbe", "ifft", "time", "out", "time", "FFT round trip", 0.5f, 2 });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const GraphAudioResult result = GraphAudioExecutor().process(
            graph,
            compileResult.plan,
            frameCount);
    const SignalPayload& saw = findNodeAudio(result, "saw").output;
    const SignalPayload& magnitude = outputForPort(findNodeAudio(result, "fft"), "mag");
    const SignalPayload& reconstructed = findNodeAudio(result, "ifft").output;

    REQUIRE(saw.traversalGrid.isValid());
    REQUIRE(magnitude.traversalGrid.isValid());
    REQUIRE(reconstructed.traversalGrid.isValid());
    REQUIRE(magnitude.traversalGrid.rows == frameCount / 2 + 1);
    REQUIRE(reconstructed.traversalGrid.columns == saw.traversalGrid.columns);
    REQUIRE(reconstructed.traversalGrid.rows == saw.traversalGrid.rows);

    const auto sawRange = std::minmax_element(
            saw.traversalGrid.values.begin(),
            saw.traversalGrid.values.end());
    INFO("saw minimum: " << *sawRange.first);
    INFO("saw maximum: " << *sawRange.second);
    REQUIRE(*sawRange.first < -0.95f);
    REQUIRE(*sawRange.second > 0.95f);

    const float fundamental = magnitude.traversalGrid.values[1];
    REQUIRE(fundamental > 0.f);
    float maximumFundamentalVariation = 0.f;
    float maximumHarmonicRatioError = 0.f;

    for (size_t column = 0; column < magnitude.traversalGrid.columns; ++column) {
        const size_t offset = column * magnitude.traversalGrid.rows;
        const float columnFundamental = magnitude.traversalGrid.values[offset + 1];
        maximumFundamentalVariation = std::max(
                maximumFundamentalVariation,
                std::abs(columnFundamental - fundamental));

        for (size_t harmonic = 2; harmonic <= 8; ++harmonic) {
            maximumHarmonicRatioError = std::max(
                    maximumHarmonicRatioError,
                    std::abs(
                            magnitude.traversalGrid.values[offset + harmonic] / columnFundamental
                            - 1.f / (float) harmonic));
        }
    }

    INFO("maximum fundamental variation: " << maximumFundamentalVariation);
    INFO("maximum harmonic ratio error: " << maximumHarmonicRatioError);
    REQUIRE(maximumFundamentalVariation < 0.002f);
    REQUIRE(maximumHarmonicRatioError < 0.002f);

    float maximumReconstructionError = 0.f;
    for (size_t sample = 0; sample < saw.traversalGrid.values.size(); ++sample) {
        maximumReconstructionError = jmax(
                maximumReconstructionError,
                std::abs(reconstructed.traversalGrid.values[sample]
                        - saw.traversalGrid.values[sample]));
    }
    REQUIRE(maximumReconstructionError < 1.0e-5f);
}

TEST_CASE("Scratch Envelope drives every attached Trimesh from one prepared trajectory",
        "[cycle-v2][runtime][envelope][scratch][trimesh]") {
    constexpr size_t frameCount = 64;
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "scratch", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "attachedA", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "attachedB", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "unattached", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "consumeA", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "consumeB", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "consumePeer", {}));
    setEnvelopePurpose(graph, "scratch", EnvelopePurpose::Scratch);

    auto mesh = TrimeshMeshFactory::createDefaultMesh("ScratchTraversalMesh");
    const auto meshState = TrimeshNodeModelState::copyOf(*mesh, 2);
    REQUIRE(graph.replaceNodeModel("attachedA", meshState));
    REQUIRE(graph.replaceNodeModel("attachedB", meshState));
    REQUIRE(graph.replaceNodeModel("unattached", meshState));
    mesh->destroy();

    for (const String& target : { "attachedA", "attachedB", "unattached" }) {
        REQUIRE(GraphEditor().setNodeParameter(
                graph, target, "yellow", "Yellow", "0.9").succeeded());
        graph.addEdge({
                "voice",
                "context",
                target,
                "context",
                PortDomain::DomainContext,
                ConnectionKind::Signal
        });
    }
    graph.addEdge({ "attachedA", "out", "consumeA", "in", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "attachedB", "out", "consumeB", "in", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "unattached", "out", "consumePeer", "in", PortDomain::TimeSignal, ConnectionKind::Signal });
    for (const String& target : { "attachedA", "attachedB" }) {
        graph.addEdge({
                "scratch",
                "env",
                target,
                "scratch",
                PortDomain::EnvelopeSignal,
                ConnectionKind::ProcessingAttachment
        });
    }
    graph.addSignalProbe({
            "scratchProbe",
            "attachedA",
            "out",
            "consumeA",
            "in",
            "Scratch target",
            0.5f,
            0
    });

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioProcessTiming timing;
    timing.sampleRate = 64.0;
    GraphAudioExecutor executor;
    const auto result = executor.process(
            graph,
            compiled.plan,
            frameCount,
            timing,
            voice);
    const auto& first = findNodeAudio(result, "attachedA").output;
    const auto& second = findNodeAudio(result, "attachedB").output;
    const auto& peer = findNodeAudio(result, "unattached").output;

    REQUIRE(first.block.samples == second.block.samples);
    REQUIRE(first.traversalGrid.values == second.traversalGrid.values);
    float blockDifference = 0.f;
    for (size_t sample = 0; sample < frameCount; ++sample) {
        blockDifference += std::abs(first.block.samples[sample] - peer.block.samples[sample]);
    }
    float gridDifference = 0.f;
    for (size_t sample = 0; sample < first.traversalGrid.values.size(); ++sample) {
        gridDifference += std::abs(
                first.traversalGrid.values[sample]
                        - peer.traversalGrid.values[sample]);
    }
    REQUIRE(blockDifference > 0.01f);
    REQUIRE(gridDifference > 0.01f);

    const auto previews = GraphPreviewExecutor().render(
            compiled.plan,
            result,
            graph.getSignalProbes(),
            frameCount);
    const auto previewFor = [&](const String& nodeId) -> const NodePreviewResult& {
        const auto found = std::find_if(
                previews.nodes.begin(),
                previews.nodes.end(),
                [&](const NodePreviewResult& preview) {
                    return preview.nodeId == nodeId;
                });
        REQUIRE(found != previews.nodes.end());
        return *found;
    };
    const auto& attachedPreview = previewFor("attachedA");
    REQUIRE(attachedPreview.primary == previewFor("attachedB").primary);
    REQUIRE(attachedPreview.primary != previewFor("unattached").primary);
    REQUIRE(previews.probes.size() == 1);
    REQUIRE(previews.probes.front().connected);
    REQUIRE(previews.probes.front().values == first.traversalGrid.values);

    const auto advanced = executor.process(
            graph,
            compiled.plan,
            frameCount,
            timing,
            {});
    const auto& advancedBlock = findNodeAudio(
            advanced, "attachedA").output.block.samples;
    float lifecycleDifference = 0.f;
    for (size_t sample = 0; sample < frameCount; ++sample) {
        lifecycleDifference += std::abs(
                advancedBlock[sample] - first.block.samples[sample]);
    }
    REQUIRE(lifecycleDifference > 0.01f);
}

TEST_CASE("Stengah scratch topology changes every authored source-layer traversal",
        "[cycle-v2][runtime][envelope][scratch][trimesh][preset]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File preset = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("stengah.cyclegraph");
    REQUIRE(preset.existsAsFile());
    const NodeGraph attachedGraph = GraphSerializer().fromJsonString(
            preset.loadFileAsString());
    NodeGraph fallbackGraph = attachedGraph;
    fallbackGraph.removeEdgesFromOutput("scratchEnvelope", "env");
    const auto attachedPlan = GraphCompiler().compile(attachedGraph);
    const auto fallbackPlan = GraphCompiler().compile(fallbackGraph);
    REQUIRE(attachedPlan.succeeded());
    REQUIRE(fallbackPlan.succeeded());

    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const auto attached = GraphAudioExecutor().process(
            attachedGraph, attachedPlan.plan, 128, {}, voice);
    const auto fallback = GraphAudioExecutor().process(
            fallbackGraph, fallbackPlan.plan, 128, {}, voice);
    for (const String& nodeId : {
            "magnitudeLayer1",
            "phaseLayer1",
            "phaseLayer2" }) {
        const auto& withScratch = findNodeAudio(attached, nodeId).output.traversalGrid.values;
        const auto& withoutScratch = findNodeAudio(fallback, nodeId).output.traversalGrid.values;
        REQUIRE(withScratch.size() == withoutScratch.size());
        float difference = 0.f;
        for (size_t sample = 0; sample < withScratch.size(); ++sample) {
            difference += std::abs(withScratch[sample] - withoutScratch[sample]);
        }
        INFO("node: " << nodeId);
        REQUIRE(difference > 0.01f);
    }
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
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
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    GraphAudioExecutor executor;
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 4;
    executor.prepareExecution(compileResult.plan, spec);
    const auto first = executor.processRealtime(compileResult.plan, 4, {}, voice);
    const auto second = executor.processRealtime(compileResult.plan, 4, {}, voice);

    REQUIRE(first.isValid());
    REQUIRE(second.isValid());
    REQUIRE(second.payload->block.samples == std::vector<float> { 0.f, 1.f / 3.f, 2.f / 3.f, 1.f });
}

TEST_CASE("Prepared graph audio processing performs no allocations or locks",
        "[cycle-v2][runtime][realtime][modulation]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::ModulationTriple,
            "allocationTriple",
            {}));
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());
    REQUIRE(std::any_of(
            compileResult.plan.steps.begin(),
            compileResult.plan.steps.end(),
            [](const GraphExecutionStep& step) {
                return step.nodeId == "allocationTriple";
            }));

    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    executor.prepareExecution(compileResult.plan, spec);
    AudioVoiceContext voice;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    REQUIRE(executor.processRealtime(compileResult.plan, 64, {}, voice).isValid());

    ScopedRealtimeAllocationCount allocations;
    ScopedRealtimeLockCount locks;
    const auto maximumOutput = executor.processRealtime(compileResult.plan, 64, {}, voice);
    const auto shorterOutput = executor.processRealtime(compileResult.plan, 32, {}, voice);
    const auto minimumOutput = executor.processRealtime(compileResult.plan, 1, {}, voice);

    REQUIRE(maximumOutput.isValid());
    REQUIRE(shorterOutput.isValid());
    REQUIRE(minimumOutput.isValid());
    REQUIRE(allocations.count() == 0);
    REQUIRE(locks.count() == 0);
}

TEST_CASE("Dynamic Envelope request and adoption remain allocation-free on the realtime path",
        "[cycle-v2][runtime][realtime][envelope][modulation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ImageSource, "redControl", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    setEnvelopePurpose(graph, "env", EnvelopePurpose::Volume);
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({
            "redControl", "out", "env", "red", PortDomain::ControlSignal, ConnectionKind::Signal
    });
    graph.addEdge({ "wave", "out", "multiply", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });
    graph.addEdge({ "multiply", "out", "output", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
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
    REQUIRE(executor.processRealtime(compiled.plan, 64, {}, {}).isValid());

    {
        ScopedRealtimeAllocationCount allocations;
        REQUIRE(executor.processRealtime(compiled.plan, 64, {}, noteOn).isValid());
        REQUIRE(allocations.count() == 0);
    }
    REQUIRE(executor.serviceNonRealtimePreparation() == 1);
    {
        ScopedRealtimeAllocationCount allocations;
        REQUIRE(executor.processRealtime(compiled.plan, 64, {}, {}).isValid());
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
    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "wave", "out", "add", "right", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "add", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    executor.prepareExecution(compiled.plan, spec);
    FanOutObserver observer;
    AudioVoiceContext voice;

    REQUIRE(executor.processRealtime(compiled.plan, 16, {}, voice, &observer).isValid());
    REQUIRE(observer.observedNodes == 3);
    REQUIRE(observer.preparedCollections);
    REQUIRE(observer.sourceSamples != nullptr);
    REQUIRE(observer.consumerSamples == observer.sourceSamples);
}
