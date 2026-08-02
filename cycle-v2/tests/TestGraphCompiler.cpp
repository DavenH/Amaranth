#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Nodes/Control/ModulationTriple.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"

#include <algorithm>

using namespace CycleV2;

namespace {

Port input(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, true };
}

Port output(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, false };
}

Node graphNode(String id, std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        id,
        NodeKind::GenericProcessor,
        {},
        {},
        {},
        std::move(inputs),
        std::move(outputs)
    };
}

int orderIndex(const GraphExecutionPlan& plan, const String& nodeId) {
    const auto found = std::find(plan.nodeOrder.begin(), plan.nodeOrder.end(), nodeId);
    REQUIRE(found != plan.nodeOrder.end());
    return static_cast<int>(std::distance(plan.nodeOrder.begin(), found));
}

const Edge& findSignalEdge(const GraphExecutionPlan& plan, const String& sourceNodeId, const String& destNodeId) {
    const auto found = std::find_if(
            plan.signalEdges.begin(),
            plan.signalEdges.end(),
            [&](const Edge& edge) {
                return edge.sourceNodeId == sourceNodeId && edge.destNodeId == destNodeId;
            });

    REQUIRE(found != plan.signalEdges.end());
    return *found;
}

const GraphExecutionStep& findStep(const GraphExecutionPlan& plan, const String& nodeId) {
    const auto found = std::find_if(
            plan.steps.begin(),
            plan.steps.end(),
            [&](const GraphExecutionStep& step) {
                return step.nodeId == nodeId;
            });

    REQUIRE(found != plan.steps.end());
    return *found;
}

int stepPlanIndex(const GraphExecutionPlan& plan, const String& nodeId) {
    const auto found = std::find_if(
            plan.steps.begin(),
            plan.steps.end(),
            [&](const GraphExecutionStep& step) {
                return step.nodeId == nodeId;
            });
    REQUIRE(found != plan.steps.end());
    return static_cast<int>(std::distance(plan.steps.begin(), found));
}

const GraphBufferPlan& findBuffer(const GraphExecutionPlan& plan, const String& sourceNodeId, const String& sourcePortId) {
    const auto found = std::find_if(
            plan.buffers.begin(),
            plan.buffers.end(),
            [&](const GraphBufferPlan& buffer) {
                return buffer.sourceNodeId == sourceNodeId && buffer.sourcePortId == sourcePortId;
            });

    REQUIRE(found != plan.buffers.end());
    return *found;
}

}

TEST_CASE("Demo graph compiles to a stable execution order", "[cycle-v2][graph]") {
    const auto result = GraphCompiler().compile(NodeGraph::createDemoGraph());

    REQUIRE(result.succeeded());
    REQUIRE(result.plan.attachments.size() == 2);
    REQUIRE(result.plan.signalEdges.size() == 11);
    REQUIRE(result.plan.buffers.size() == 11);
    REQUIRE(result.plan.steps.size() == result.plan.nodeOrder.size());
    REQUIRE(result.plan.voiceContexts.size() == 1);

    const auto& plan = result.plan;
    REQUIRE(orderIndex(plan, "voice") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "waveMesh") < orderIndex(plan, "fft"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "magMesh"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "magMesh") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "phaseMesh") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "addMag") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "addPhase") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "ifft") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "env") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "multiply") < orderIndex(plan, "out"));

    REQUIRE(parameterValueForNode({ "voice", NodeKind::VoiceContext, {}, {}, findStep(plan, "voice").parameters, {}, {} },
            "domain") == "waveform");
    REQUIRE(findStep(plan, "waveMesh").audioRole == AudioModuleRole::MeshSource);
    REQUIRE(findStep(plan, "waveMesh").previewRole == PreviewModuleRole::MeshSurface);
    REQUIRE(findStep(plan, "waveMesh").cycle1AdapterBacked);
    REQUIRE(findStep(plan, "waveMesh").cycle1Reference
            == "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp");
    REQUIRE(findStep(plan, "fft").audioRole == AudioModuleRole::Fft);
    REQUIRE_FALSE(findStep(plan, "fft").previewable);
    REQUIRE(findStep(plan, "fft").cycleFrames == 2048);
    REQUIRE(findStep(plan, "fft").latencyCycles == 0);
    REQUIRE(findStep(plan, "fft").transformMode == "cycle");
    REQUIRE(findStep(plan, "ifft").cycleFrames == 2048);
    REQUIRE(findStep(plan, "ifft").latencyCycles == 0);
    REQUIRE(findStep(plan, "ifft").transformMode == "cyclic");
    REQUIRE(findStep(plan, "multiply").audioRole == AudioModuleRole::Multiply);
    REQUIRE_FALSE(findStep(plan, "out").previewable);
    REQUIRE(findStep(plan, "out").previewRole == PreviewModuleRole::None);
    REQUIRE(findStep(plan, "multiply").inputs.size() == 2);
    REQUIRE(findStep(plan, "multiply").inputs[0].destPortId == "left");
    REQUIRE(findStep(plan, "multiply").inputs[0].destPortIndex == 0);
    REQUIRE(findStep(plan, "multiply").inputs[0].domain == PortDomain::TimeSignal);
    REQUIRE(findStep(plan, "multiply").inputs[0].channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(findStep(plan, "multiply").inputs[1].destPortId == "right");
    REQUIRE(findStep(plan, "multiply").inputs[1].destPortIndex == 1);
    REQUIRE(findStep(plan, "multiply").inputs[1].domain == PortDomain::EnvelopeSignal);
    REQUIRE(findSignalEdge(plan, "magMesh", "addMag").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findSignalEdge(plan, "phaseMesh", "addPhase").domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findBuffer(plan, "magMesh", "out").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findBuffer(plan, "phaseMesh", "out").domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findBuffer(plan, "ifft", "time").domain == PortDomain::TimeSignal);
    REQUIRE(findBuffer(plan, "ifft", "time").channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(plan.oscillatorRegions.size() == 1);
    const auto& region = plan.oscillatorRegions.front();
    REQUIRE(region.voiceContextNodeId == "voice");
    REQUIRE(region.strategy == OscillatorExecutionStrategy::SharedSpectralFrame);
    REQUIRE(region.reconstruction == SpectralReconstructionPolicy::CyclicFrameCrossfade);
    REQUIRE(region.laneCount == 1);
    REQUIRE(region.materializationStepIndex == stepPlanIndex(plan, "ifft"));
    REQUIRE(findStep(plan, "waveMesh").executionCoordinate
            == ExecutionCoordinate::CycleField);
    REQUIRE(findStep(plan, "fft").executionCoordinate
            == ExecutionCoordinate::SpectralFrame);
    REQUIRE(findStep(plan, "magMesh").executionCoordinate
            == ExecutionCoordinate::SpectralFrame);
    REQUIRE(findStep(plan, "ifft").executionCoordinate
            == ExecutionCoordinate::SampleBlock);
    REQUIRE(findStep(plan, "multiply").executionCoordinate
            == ExecutionCoordinate::SampleBlock);
}

TEST_CASE("Compiler plans a time-only oscillator region per Unison lane",
        "[cycle-v2][graph][oscillator-region][unison]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Unison, "unison", {}));
    Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    graph.addNode(std::move(mesh));
    REQUIRE(GraphEditor().setNodeParameter(
            graph, "unison", "order", "Voices", "4").succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "unison", "unison", false },
            { "voice", "unison", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "mesh", "context", true }).succeeded());

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(result.plan.oscillatorRegions.size() == 1);
    const auto& region = result.plan.oscillatorRegions.front();
    REQUIRE(region.strategy == OscillatorExecutionStrategy::ChainedPerLane);
    REQUIRE(region.laneCount == 4);
    REQUIRE(region.materializationStepIndex == stepPlanIndex(result.plan, "mesh"));
    REQUIRE(findStep(result.plan, "mesh").ownershipScope
            == RuntimeOwnershipScope::UnisonLane);
    REQUIRE(findStep(result.plan, "mesh").executionCoordinate
            == ExecutionCoordinate::CycleField);
}

TEST_CASE("Compiler rejects oscillator operations reached by two Voice Contexts",
        "[cycle-v2][graph][oscillator-region]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "firstVoice", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "secondVoice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "firstMesh", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "secondMesh", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "merge", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "firstVoice", "context", false },
            { "firstMesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "secondVoice", "context", false },
            { "secondMesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "firstMesh", "out", false },
            { "merge", "left", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "secondMesh", "out", false },
            { "merge", "right", true }).succeeded());

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.compileIssues.size() == 1);
    REQUIRE(result.compileIssues.front().code == GraphCompileCode::AmbiguousVoiceContext);
    REQUIRE(result.compileIssues.front().message.contains("merge"));
}

TEST_CASE("Compiler materializes sibling spectral and chained oscillator regions before Add",
        "[cycle-v2][graph][oscillator-region][materialization]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "spectralMesh", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "chainedMesh", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "spectralMesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "spectralMesh", "out", false },
            { "fft", "time", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "mag", false },
            { "ifft", "mag", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "fft", "phase", false },
            { "ifft", "phase", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "chainedMesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "ifft", "time", false },
            { "add", "left", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph,
            { "chainedMesh", "out", false },
            { "add", "right", true }).succeeded());

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(result.plan.oscillatorRegions.size() == 2);
    REQUIRE(result.plan.oscillatorRegions[0].strategy
            == OscillatorExecutionStrategy::SharedSpectralFrame);
    REQUIRE(result.plan.oscillatorRegions[0].materializationStepIndex
            == stepPlanIndex(result.plan, "ifft"));
    REQUIRE(result.plan.oscillatorRegions[1].strategy
            == OscillatorExecutionStrategy::ChainedPerLane);
    REQUIRE(result.plan.oscillatorRegions[1].materializationStepIndex
            == stepPlanIndex(result.plan, "chainedMesh"));
    REQUIRE(findStep(result.plan, "add").oscillatorRegionIndex == -1);
    REQUIRE(findStep(result.plan, "add").executionCoordinate
            == ExecutionCoordinate::SampleBlock);
}

TEST_CASE("Voice Context defaults resolve per axis with explicit override precedence",
        "[cycle-v2][graph][voice-context][modulation]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    Node triple = factory.createNode(NodeKind::ModulationTriple, "triple", {});
    for (auto& parameter : triple.parameters) {
        if (parameter.id == "yellowSource" || parameter.id == "blueSource") {
            parameter.value = "constant";
        } else if (parameter.id == "yellowConstant") {
            parameter.value = "0.2";
        } else if (parameter.id == "blueConstant") {
            parameter.value = "0.8";
        }
    }
    graph.addNode(std::move(triple));
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "explicitRed", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    REQUIRE(GraphEditor().connect(
            graph, { "triple", "modulation", false }, { "voice", "modulation", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph, { "voice", "context", false }, { "mesh", "context", true }).succeeded());
    REQUIRE(GraphEditor().connect(
            graph, { "explicitRed", "value", false }, { "mesh", "red", true }).succeeded());

    const auto compiled = GraphCompiler().compile(graph);

    REQUIRE(compiled.succeeded());
    const auto& mesh = findStep(compiled.plan, "mesh");
    REQUIRE(std::count_if(mesh.inputs.begin(), mesh.inputs.end(), [](const auto& input) {
        return input.sourcePortId.startsWith("default.");
    }) == 2);
    REQUIRE(std::any_of(mesh.inputs.begin(), mesh.inputs.end(), [](const auto& input) {
        return input.destPortId == "red" && input.sourceNodeId == "explicitRed";
    }));
    const auto implicit = std::find_if(
            compiled.plan.buffers.begin(),
            compiled.plan.buffers.end(),
            [](const auto& buffer) {
                return buffer.defaultModulationSlot == DefaultModulationSlot::Yellow;
            });
    REQUIRE(implicit != compiled.plan.buffers.end());
    const auto configuration = std::dynamic_pointer_cast<const ModulationTripleConfiguration>(
            implicit->defaultModulation);
    REQUIRE(configuration != nullptr);
    REQUIRE(configuration->sources[0].constant == Catch::Approx(0.2f));
}

TEST_CASE("Compiler indexes both dependency directions and probe addresses",
        "[cycle-v2][graph][runtime]") {
    NodeGraph graph;
    graph.addNode(graphNode(
            "source", {}, { output("signal", PortDomain::TimeSignal) }));
    graph.addNode(graphNode(
            "sink", { input("in", PortDomain::TimeSignal) }, {}));
    graph.addEdge({
            "source", "signal", "sink", "in", PortDomain::TimeSignal, false });
    graph.addSignalProbe({ "probe", "source", "signal", "sink", "in", "Probe" });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    const auto& index = result.plan.dependencyIndex;
    REQUIRE(index.nodeIndexById.at("source") == 0);
    REQUIRE(index.nodeIndexById.at("sink") == 1);
    REQUIRE((index.dependents == std::vector<std::vector<int>> { { 1 }, {} }));
    REQUIRE((index.dependencies == std::vector<std::vector<int>> { {}, { 0 } }));
    REQUIRE(result.plan.signalProbes.size() == 1);
    CHECK(result.plan.signalProbes.front().probeId == "probe");
    CHECK(result.plan.signalProbes.front().sourceStepIndex == 0);
    CHECK(result.plan.signalProbes.front().sourceOutputIndex == 0);
}

TEST_CASE("Compiler publishes stable waveshaper DSP configurations", "[cycle-v2][graph][configuration]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", { 0.f, 0.f }));
    GraphCompiler compiler;

    const auto first = compiler.compile(graph);
    const auto unchanged = compiler.compile(graph);
    REQUIRE(GraphEditor().setNodeParameter(graph, "shape", "pre", "Pre", "0.75").succeeded());
    const auto changed = compiler.compile(graph);

    REQUIRE(first.succeeded());
    REQUIRE(first.plan.steps.front().configuration.isValid());
    REQUIRE(unchanged.plan.steps.front().configuration.revision
            == first.plan.steps.front().configuration.revision);
    REQUIRE(unchanged.plan.steps.front().configuration.value
            == first.plan.steps.front().configuration.value);
    REQUIRE(changed.plan.steps.front().configuration.revision
            == first.plan.steps.front().configuration.revision + 1);
    REQUIRE(changed.plan.steps.front().configuration.value
            != first.plan.steps.front().configuration.value);
}

TEST_CASE("Compiler declares IFFT carry-buffer latency", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 0.f, 0.f }));
    graph.replaceNodeParameters("ifft", {
            { "cycleFrames", "Cycle Frames", "4096" },
            { "mode", "Mode", "acyclicCarry" }
    });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findStep(result.plan, "ifft").cycleFrames == 4096);
    REQUIRE(findStep(result.plan, "ifft").latencyCycles == 1);
    REQUIRE(findStep(result.plan, "ifft").transformMode == "acyclicCarry");
}

TEST_CASE("Compiler preserves FFT fixed-window mode", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 0.f, 0.f }));
    graph.replaceNodeParameters("fft", {
            { "cycleFrames", "Cycle Frames", "4096" },
            { "mode", "Mode", "fixedWindow" }
    });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findStep(result.plan, "fft").cycleFrames == 4096);
    REQUIRE(findStep(result.plan, "fft").latencyCycles == 0);
    REQUIRE(findStep(result.plan, "fft").transformMode == "fixedWindow");
}

TEST_CASE("Invalid graphs do not compile", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "voice", "context", "multiply", "audio", PortDomain::DomainContext, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.empty());
    REQUIRE(result.plan.nodeOrder.empty());
}

TEST_CASE("Compiler resolves source domains from voice context parameters", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "next", { 760.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });
    graph.addEdge({ "add", "out", "next", "left", PortDomain::ControlSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findSignalEdge(result.plan, "mesh", "add").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findSignalEdge(result.plan, "add", "next").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findStep(result.plan, "add").inputs.front().domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findBuffer(result.plan, "mesh", "out").domain == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("One spectral voice context resolves magnitude and phase mesh branches",
        "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    voice.parameters = {
            { "domain", "Start Domain", "spectral" }
    };

    graph.addNode(std::move(voice));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "magnitude", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "phaseA", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "phaseB", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "phaseAdd", {}));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({ "voice", "context", "magnitude", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "voice", "context", "phaseA", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "voice", "context", "phaseB", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "magnitude", "out", "ifft", "mag", PortDomain::ControlSignal, false });
    graph.addEdge({ "phaseA", "out", "phaseAdd", "left", PortDomain::ControlSignal, false });
    graph.addEdge({ "phaseB", "out", "phaseAdd", "right", PortDomain::ControlSignal, false });
    graph.addEdge({ "phaseAdd", "out", "ifft", "phase", PortDomain::ControlSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findBuffer(result.plan, "magnitude", "out").domain
            == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findBuffer(result.plan, "phaseA", "out").domain
            == PortDomain::SpectralPhaseSignal);
    REQUIRE(findBuffer(result.plan, "phaseB", "out").domain
            == PortDomain::SpectralPhaseSignal);
    REQUIRE(findSignalEdge(result.plan, "phaseAdd", "ifft").domain
            == PortDomain::SpectralPhaseSignal);
}

TEST_CASE("Compiler keeps wave source fixed in the time domain", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 520.f, 0.f }));
    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findSignalEdge(result.plan, "wave", "add").domain == PortDomain::TimeSignal);
    REQUIRE(findStep(result.plan, "add").inputs[0].domain == PortDomain::TimeSignal);
    REQUIRE(findBuffer(result.plan, "wave", "out").domain == PortDomain::TimeSignal);
}

TEST_CASE("Compiler resolves mesh output domains from consuming operation context", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            {},
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 520.f, 0.f }));
    graph.addEdge({ "mag", "out", "add", "left", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE(result.succeeded());
    REQUIRE(findSignalEdge(result.plan, "mesh", "add").domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findBuffer(result.plan, "mesh", "out").domain == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Compiler rejects processing cycles", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(graphNode(
            "a",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addNode(graphNode(
            "b",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addEdge({ "a", "out", "b", "in", PortDomain::TimeSignal, false });
    graph.addEdge({ "b", "out", "a", "in", PortDomain::TimeSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.size() == 1);
    REQUIRE(result.compileIssues.front().code == GraphCompileCode::CycleDetected);
    REQUIRE(result.plan.nodeOrder.empty());
}

TEST_CASE("Compiler assigns output slots and source lifetimes before processing", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal, false });

    const auto result = GraphCompiler().compile(graph);
    REQUIRE(result.succeeded());

    const auto& wave = findStep(result.plan, "wave");
    const auto& add = findStep(result.plan, "add");
    REQUIRE(wave.outputs.front().bufferIndex >= 0);
    REQUIRE(add.inputs.front().sourceBufferIndex == wave.outputs.front().bufferIndex);
    REQUIRE(add.inputs.front().sourceStepIndex >= 0);
    REQUIRE(result.plan.steps[(size_t) add.inputs.front().sourceStepIndex].nodeId == "wave");
    REQUIRE(add.inputs.front().sourceOutputIndex == 0);
    const auto& buffer = result.plan.buffers[(size_t) wave.outputs.front().bufferIndex];
    REQUIRE(buffer.firstProducerStep >= 0);
    REQUIRE(buffer.lastConsumerStep > buffer.firstProducerStep);
}
