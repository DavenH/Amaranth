#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Control/ModulationSource.h"
#include "../src/Nodes/Control/ModulationTriple.h"
#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Runtime/AudioProcessContextUtils.h"
#include "../src/Runtime/GraphAudioExecutor.h"
#include "../src/Runtime/MidiControlState.h"
#include "../src/UI/NodeEditorHost.h"
#include "../src/UI/ModulationCableBundle.h"
#include "../src/UI/NodeCanvasInteraction.h"
#include "../src/UI/NodeCanvasScene.h"
#include "../src/UI/NodeViewModule.h"

using namespace CycleV2;

namespace {

std::shared_ptr<const ModulationSourceConfiguration> configuration(
        const String& source,
        int controller = 1,
        float constant = 0.5f) {
    return ModulationSource::buildConfiguration({
            { "source", "Source", source },
            { "controller", "Controller", String(controller) },
            { "constant", "Constant", String(constant) }
    });
}

AudioProcessContext processContext(
        size_t frameCount,
        const AudioVoiceContext& voice) {
    AudioProcessContext context;
    context.frameCount = frameCount;
    context.voiceView = &voice;
    context.outputPorts.push_back({ "value", PortDomain::ControlSignal, ChannelLayout::Mono });
    return context;
}

std::vector<float> renderAudio(
        const ModulationSourceConfiguration& configurationToUse,
        const AudioVoiceContext& voice,
        size_t frameCount) {
    auto processor = createModulationSourceAudioProcessor();
    const auto ownership = std::make_shared<ModulationSourceConfiguration>(configurationToUse);
    processor->adoptConfiguration({ 1, "modulation", ownership });
    auto context = processContext(frameCount, voice);
    processor->process(context);
    REQUIRE(context.outputs.size() == 1);
    const auto& samples = context.outputs.front().block.samples;
    return { samples.begin(), samples.end() };
}

}

TEST_CASE("Modulation source evaluates Cycle v1 control semantics",
        "[cycle-v2][modulation][control]") {
    PreviewControlContext context;
    context.voiceTime = 0.25f;
    context.velocity = 0.2f;
    context.noteNumber = 64;
    context.lowestNote = 0;
    context.highestNote = 127;
    context.channelPressure = 0.75f;
    context.controllers[1] = 0.3f;
    context.controllers[74] = 0.6f;

    REQUIRE(ModulationSource::evaluate(*configuration("voiceTime"), context) == 0.25f);
    REQUIRE(ModulationSource::evaluate(*configuration("velocity"), context) == 0.2f);
    REQUIRE(ModulationSource::evaluate(*configuration("inverseVelocity"), context)
            == Catch::Approx(0.8f));
    REQUIRE(ModulationSource::evaluate(*configuration("keyScale"), context)
            == Catch::Approx(64.f / 127.f));
    REQUIRE(ModulationSource::evaluate(*configuration("modWheel"), context) == 0.3f);
    REQUIRE(ModulationSource::evaluate(*configuration("midiCC", 74), context) == 0.6f);
    REQUIRE(ModulationSource::evaluate(*configuration("channelPressure"), context) == 0.75f);
    REQUIRE(ModulationSource::evaluate(*configuration("constant", 1, 0.4f), context) == 0.4f);

    REQUIRE(ModulationSource::normalizeKey(10, 10, 10) == 0.f);
    REQUIRE(ModulationSource::normalizeKey(-10, 0, 127) == 0.f);
    REQUIRE(ModulationSource::normalizeKey(140, 0, 127) == 1.f);
}

TEST_CASE("Mod wheel and MIDI CC 1 share controller state",
        "[cycle-v2][modulation][control]") {
    PreviewControlContext context;
    context.controllers[1] = 0.73f;

    REQUIRE(ModulationSource::evaluate(*configuration("modWheel"), context)
            == ModulationSource::evaluate(*configuration("midiCC", 1), context));
}

TEST_CASE("MIDI endpoints normalize for every supported control kind",
        "[cycle-v2][modulation][control]") {
    MidiControlState state;
    state.prepare(8);
    state.beginBlock();
    state.ingest(MidiMessage::controllerEvent(1, 0, 0), 0);
    state.ingest(MidiMessage::controllerEvent(1, 127, 127), 1);
    state.ingest(MidiMessage::channelPressureChange(1, 127), 2);
    AudioVoiceContext voice;
    state.prepareVoice(voice);
    state.populateVoice(voice, 1);

    REQUIRE(voice.controlEvents[0].controller == 0);
    REQUIRE(voice.controlEvents[0].value == 0.f);
    REQUIRE(voice.controlEvents[1].controller == 127);
    REQUIRE(voice.controlEvents[1].value == 1.f);
    REQUIRE(voice.controlEvents[2].kind == ControlEventKind::ChannelPressure);
    REQUIRE(voice.controlEvents[2].value == 1.f);
}

TEST_CASE("Timed controller changes produce sample-accurate held spans",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.controllers[74] = 0.1f;
    voice.controlEvents = {
            { ControlEventKind::Controller, 2, 74, 0.5f },
            { ControlEventKind::Controller, 5, 74, 0.9f }
    };

    const auto output = renderAudio(*configuration("midiCC", 74), voice, 8);
    REQUIRE(output == std::vector<float> {
            0.1f, 0.1f, 0.5f, 0.5f, 0.5f, 0.9f, 0.9f, 0.9f
    });
}

TEST_CASE("Voice time advances at audio rate and clamps at its normalized endpoint",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.normalizedVoiceTime = 0.25f;
    voice.controls.normalizedVoiceTimeIncrement = 0.2f;

    REQUIRE(renderAudio(*configuration("voiceTime"), voice, 6)
            == std::vector<float> { 0.25f, 0.45f, 0.65f, 0.85f, 1.f, 1.f });
}

TEST_CASE("Unrelated controller events do not change a selected source",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.controllers[1] = 0.4f;
    voice.controlEvents = {
            { ControlEventKind::Controller, 3, 74, 1.f }
    };

    const auto output = renderAudio(*configuration("modWheel"), voice, 6);
    REQUIRE(output == std::vector<float>(6, 0.4f));
}

TEST_CASE("Channel pressure events are independent of MIDI controllers",
        "[cycle-v2][modulation][audio]") {
    AudioVoiceContext voice;
    voice.controls.channelPressure = 0.2f;
    voice.controls.controllers[1] = 0.8f;
    voice.controlEvents = {
            { ControlEventKind::ChannelPressure, 3, 0, 0.7f }
    };

    REQUIRE(renderAudio(*configuration("channelPressure"), voice, 5)
            == std::vector<float> { 0.2f, 0.2f, 0.2f, 0.7f, 0.7f });
    REQUIRE(renderAudio(*configuration("modWheel"), voice, 5)
            == std::vector<float>(5, 0.8f));
}

TEST_CASE("Modulation preview is deterministic from explicit audition controls",
        "[cycle-v2][modulation][preview]") {
    PreviewControlContext controls;
    controls.noteNumber = 96;
    controls.lowestNote = 0;
    controls.highestNote = 127;
    auto processor = createModulationSourcePreviewProcessor();
    PreviewProcessContext context;
    context.pointCount = 7;
    context.parameters = {
            { "source", "Source", "keyScale" }
    };
    context.controlContext = &controls;

    processor->render(context);

    REQUIRE(context.domain == PortDomain::ControlSignal);
    REQUIRE(context.primary == std::vector<float>(7, 96.f / 127.f));
    controls.noteNumber = 12;
    REQUIRE(context.primary == std::vector<float>(7, 96.f / 127.f));
}

TEST_CASE("MIDI control state preserves block start values and ordered events",
        "[cycle-v2][modulation][midi]") {
    MidiControlState state;
    state.prepare(8);
    state.beginBlock();
    state.ingest(MidiMessage::controllerEvent(2, 74, 32), 2);
    state.ingest(MidiMessage::channelPressureChange(2, 96), 5);
    AudioVoiceContext first;
    state.prepareVoice(first);
    state.populateVoice(first, 2);

    REQUIRE(first.controls.controllers[74] == 0.f);
    REQUIRE(first.controls.channelPressure == 0.f);
    REQUIRE(first.controlEvents.size() == 2);
    REQUIRE(first.controlEvents[0].sampleOffset == 2);
    REQUIRE(first.controlEvents[1].sampleOffset == 5);

    state.beginBlock();
    AudioVoiceContext next;
    state.prepareVoice(next);
    state.populateVoice(next, 2);
    REQUIRE(next.controls.controllers[74] == Catch::Approx(32.f / 127.f));
    REQUIRE(next.controls.channelPressure == Catch::Approx(96.f / 127.f));
    REQUIRE(next.controlEvents.empty());

    AudioVoiceContext otherChannel;
    state.prepareVoice(otherChannel);
    state.populateVoice(otherChannel, 1);
    REQUIRE(otherChannel.controls.controllers[74] == 0.f);
    REQUIRE(otherChannel.controls.channelPressure == 0.f);
}

TEST_CASE("MIDI control state bounds event storage and reports overflow",
        "[cycle-v2][modulation][midi]") {
    MidiControlState state;
    state.prepare(1);
    state.beginBlock();
    state.ingest(MidiMessage::controllerEvent(1, 1, 32), 1);
    state.ingest(MidiMessage::controllerEvent(1, 1, 96), 2);

    AudioVoiceContext voice;
    state.prepareVoice(voice);
    state.populateVoice(voice, 1);
    REQUIRE(voice.controlEvents.size() == 1);
    REQUIRE(state.droppedEventCount() == 1);

    state.beginBlock();
    REQUIRE(state.droppedEventCount() == 0);
    state.populateVoice(voice, 1);
    REQUIRE(voice.controls.controllers[1] == Catch::Approx(96.f / 127.f));
}

TEST_CASE("Voice-time preview traversal is explicit and stateless",
        "[cycle-v2][modulation][preview]") {
    auto processor = createModulationSourcePreviewProcessor();
    PreviewControlContext controls;
    controls.voiceTime = 0.4f;
    PreviewProcessContext scalar;
    scalar.pointCount = 5;
    scalar.parameters = { { "source", "Source", "voiceTime" } };
    scalar.controlContext = &controls;
    processor->render(scalar);
    REQUIRE(scalar.primary == std::vector<float>(5, 0.4f));

    controls.traverseVoiceTime = true;
    PreviewProcessContext traversal;
    traversal.pointCount = 5;
    traversal.parameters = scalar.parameters;
    traversal.controlContext = &controls;
    processor->render(traversal);
    REQUIRE(traversal.primary == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(traversal.gridColumns == 5);
    REQUIRE(traversal.gridRows == 1);
}

TEST_CASE("Modulation nodes compile, fan out, and round-trip as graph routes",
        "[cycle-v2][modulation][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "cc", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.replaceNodeParameters("cc", {
            { "source", "Source", "midiCC" },
            { "controller", "Controller", "74" },
            { "constant", "Constant", "0.5" }
    });
    graph.addEdge({ "cc", "value", "env", "red", PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "cc", "value", "mesh", "blue", PortDomain::ControlSignal, ConnectionKind::Signal });

    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const GraphSerializer serializer;
    const GraphLoadResult loaded = serializer.loadJsonString(serializer.toJsonString(graph));
    REQUIRE(loaded.succeeded());
    const NodeGraph& restored = loaded.graph;
    REQUIRE(restored.getEdges().size() == 2);
    const Node* source = restored.findNode("cc");
    REQUIRE(source != nullptr);
    REQUIRE(source->kind == NodeKind::ModulationSource);
    REQUIRE(parameterValueForNode(*source, "source") == "midiCC");
    REQUIRE(parameterValueForNode(*source, "controller") == "74");
}

TEST_CASE("Ordinary graph editing routes modulation to every morph input and replaces conflicts",
        "[cycle-v2][modulation][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "first", {}));
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "second", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    GraphEditor editor;

    for (const String portId : { String("red"), String("blue") }) {
        REQUIRE(editor.connect(
                graph,
                { "first", "value", false },
                { "env", portId, true }).succeeded());
    }
    for (const String portId : { String("yellow"), String("red"), String("blue") }) {
        REQUIRE(editor.connect(
                graph,
                { "first", "value", false },
                { "mesh", portId, true }).succeeded());
    }
    REQUIRE(GraphCompiler().compile(graph).succeeded());
    REQUIRE(graph.getEdges().size() == 5);

    REQUIRE(editor.connect(
            graph,
            { "second", "value", false },
            { "env", "red", true }).succeeded());
    REQUIRE(graph.getEdges().size() == 5);
    const auto incoming = std::count_if(
            graph.getEdges().begin(),
            graph.getEdges().end(),
            [](const Edge& edge) {
                return edge.destNodeId == "env" && edge.destPortId == "red";
            });
    REQUIRE(incoming == 1);
}

TEST_CASE("Graph execution delivers per-voice modulation to connected destinations",
        "[cycle-v2][modulation][graph][audio]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "velocity", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.replaceNodeParameters("velocity", {
            { "source", "Source", "velocity" },
            { "controller", "Controller", "1" },
            { "constant", "Constant", "0.5" }
    });
    graph.addEdge({
            "velocity", "value", "env", "red", PortDomain::ControlSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphAudioExecutor executor;
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 16;
    executor.prepareExecution(compiled.plan, spec, 3);
    AudioVoiceContext voice;
    voice.voiceIndex = 3;
    voice.controls.velocity = 0.35f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 3 });

    const auto result = executor.process(graph, compiled.plan, 16, {}, voice);
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [](const NodeAudioResult& node) { return node.nodeId == "velocity"; });
    REQUIRE(found != result.nodes.end());
    REQUIRE(found->output.block.samples == std::vector<float>(16, 0.35f));
}

TEST_CASE("Modulation node exposes its hosted authoring editor",
        "[cycle-v2][modulation][ui]") {
    REQUIRE(NodeEditorFactoryRegistry::instance().find(NodeKind::ModulationSource) != nullptr);
    const auto& capabilities = NodeViewModuleRegistry::instance()
            .moduleFor(NodeKind::ModulationSource)
            .capabilities();
    REQUIRE(capabilities.previewable);
    REQUIRE(capabilities.hostedEditor);
}

TEST_CASE("Modulation triple reuses source semantics across ordinary outputs",
        "[cycle-v2][modulation][triple][audio]") {
    auto processor = createModulationTripleAudioProcessor();
    const auto tripleConfiguration = buildModulationTripleConfiguration({
            { "yellowSource", "Yellow Source", "voiceTime" },
            { "redSource", "Red Source", "keyScale" },
            { "blueSource", "Blue Source", "midiCC" },
            { "blueController", "Blue Controller", "74" }
    });
    processor->adoptConfiguration({ 1, "triple", tripleConfiguration });

    AudioVoiceContext voice;
    voice.controls.normalizedVoiceTime = 0.1f;
    voice.controls.normalizedVoiceTimeIncrement = 0.1f;
    voice.controls.noteNumber = 63;
    voice.controls.lowestNote = 0;
    voice.controls.highestNote = 126;
    voice.controls.controllers[74] = 0.25f;
    voice.controlEvents.push_back({ ControlEventKind::Controller, 2, 74, 0.75f });

    AudioProcessContext context;
    context.frameCount = 4;
    context.voiceView = &voice;
    context.outputPorts = {
            { "yellow", PortDomain::ControlSignal, ChannelLayout::Mono },
            { "red", PortDomain::ControlSignal, ChannelLayout::Mono },
            { "blue", PortDomain::ControlSignal, ChannelLayout::Mono }
    };
    processor->process(context);

    REQUIRE(context.outputs.size() == 3);
    REQUIRE(context.outputs[0].block.samples
            == std::vector<float> { 0.1f, 0.2f, 0.3f, 0.4f });
    REQUIRE(context.outputs[1].block.samples == std::vector<float>(4, 0.5f));
    REQUIRE(context.outputs[2].block.samples
            == std::vector<float> { 0.25f, 0.25f, 0.75f, 0.75f });
}

TEST_CASE("Modulation triple definition retains Cycle v1 axis defaults",
        "[cycle-v2][modulation][triple][graph]") {
    const Node triple = GraphNodeFactory().createNode(
            NodeKind::ModulationTriple,
            "triple",
            {});

    REQUIRE(triple.outputs.size() == 4);
    REQUIRE(triple.outputs.back().connectionKind == ConnectionKind::ConfigurationAttachment);
    REQUIRE(triple.outputs.back().attachmentType == AttachmentType::ModulationTriple);
    REQUIRE(triple.outputs.back().side == PortSide::Right);
    REQUIRE(parameterValueForNode(triple, "yellowSource") == "voiceTime");
    REQUIRE(parameterValueForNode(triple, "redSource") == "keyScale");
    REQUIRE(parameterValueForNode(triple, "blueSource") == "modWheel");
    REQUIRE(triple.bounds.getHeight() == 126.f);
}

TEST_CASE("Modulation triple side socket authors the Voice Context default attachment",
        "[cycle-v2][modulation][triple][voice-context][ui]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(
            NodeKind::ModulationTriple,
            "triple",
            { 10.f, 20.f }));
    graph.addNode(factory.createNode(
            NodeKind::VoiceContext,
            "voice",
            { 420.f, 20.f }));
    const Node* triple = graph.findNode("triple");
    const Node* voice = graph.findNode("voice");
    REQUIRE(triple != nullptr);
    REQUIRE(voice != nullptr);

    const PortAddress source = ModulationCableBundle::sourceAddress(*triple);
    const PortAddress destination { "voice", "modulation", true };
    const auto routes = ModulationCableBundle::routes(graph, source, destination);

    REQUIRE(source.portId == ModulationCableBundle::portId());
    REQUIRE(routes.size() == 1);
    REQUIRE(routes.front().source.nodeId == source.nodeId);
    REQUIRE(routes.front().source.portId == "modulation");
    REQUIRE_FALSE(routes.front().source.input);
    REQUIRE(routes.front().destination.nodeId == destination.nodeId);
    REQUIRE(routes.front().destination.portId == destination.portId);
    REQUIRE(routes.front().destination.input);
    REQUIRE(ModulationCableBundle::canConnect(graph, source, destination));

    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1000.f, 700.f });
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    const auto tripleOutputs = std::count_if(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::OutputPort
                        && target.nodeId == "triple";
            });
    REQUIRE(tripleOutputs == 1);
    const auto tripleOutput = std::find_if(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::OutputPort
                        && target.nodeId == "triple";
            });
    const auto voiceInput = std::find_if(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::InputPort
                        && target.nodeId == "voice"
                        && target.portId == "modulation";
            });
    REQUIRE(tripleOutput != snapshot.targets.end());
    REQUIRE(voiceInput != snapshot.targets.end());
    NodeCanvasInteraction interaction;
    const PortAddress draggedSource {
            tripleOutput->nodeId,
            tripleOutput->portId,
            false
    };
    const auto resolvedInput = interaction.connectionTargetAt(
            graph,
            snapshot,
            draggedSource,
            voiceInput->bounds.getCentre());
    REQUIRE(resolvedInput.has_value());
    REQUIRE(resolvedInput->nodeId == "voice");
    REQUIRE(resolvedInput->portId == "modulation");
    REQUIRE(resolvedInput->input);

    REQUIRE(GraphEditor().connect(
            graph,
            routes.front().source,
            routes.front().destination).succeeded());
    REQUIRE(graph.getEdges().size() == 1);
    REQUIRE(graph.getEdges().front().connectionKind
            == ConnectionKind::ConfigurationAttachment);
    REQUIRE(graph.getEdges().front().attachmentType
            == AttachmentType::ModulationTriple);
    const auto& connectedSnapshot = scene.build(graph, viewport);
    REQUIRE(connectedSnapshot.edges.size() == 1);
    REQUIRE_FALSE(connectedSnapshot.edges.front().modulationBundle);
    triple = graph.findNode("triple");
    REQUIRE(triple != nullptr);
    const Point<float> expectedSource = viewport.toScreen(
            ModulationCableBundle::worldCentre(*triple, false));
    REQUIRE(connectedSnapshot.edges.front().source.x == Catch::Approx(expectedSource.x));
    REQUIRE(connectedSnapshot.edges.front().source.y == Catch::Approx(expectedSource.y));
}

TEST_CASE("Matching triple routes coalesce into one canvas cable",
        "[cycle-v2][modulation][triple][ui]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(
            NodeKind::ModulationTriple,
            "triple",
            { 10.f, 20.f }));
    graph.addNode(factory.createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            { 420.f, 20.f }));
    const Node* triple = graph.findNode("triple");
    const Node* mesh = graph.findNode("mesh");
    REQUIRE(triple != nullptr);
    REQUIRE(mesh != nullptr);

    const PortAddress source = ModulationCableBundle::sourceAddress(*triple);
    const PortAddress destination = ModulationCableBundle::destinationAddress(*mesh);
    REQUIRE(ModulationCableBundle::canConnect(graph, source, destination));
    for (const auto& route : ModulationCableBundle::routes(graph, source, destination)) {
        REQUIRE(GraphEditor().connect(graph, route.source, route.destination).succeeded());
    }
    REQUIRE(graph.getEdges().size() == 3);

    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1000.f, 700.f });
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().modulationBundle);
    REQUIRE(snapshot.edges.front().edgeIndices.size() == 3);

    const auto bundleTargetCount = std::count_if(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.portId == ModulationCableBundle::portId();
            });
    REQUIRE(bundleTargetCount == 2);
    const auto hasMeshInputTarget = [&](const String& portId) {
        return std::any_of(
                snapshot.targets.begin(),
                snapshot.targets.end(),
                [&](const NodeSceneTarget& target) {
                    return target.kind == NodeSceneTargetKind::InputPort
                            && target.nodeId == "mesh"
                            && target.portId == portId;
                });
    };
    REQUIRE_FALSE(hasMeshInputTarget("yellow"));
    REQUIRE_FALSE(hasMeshInputTarget("red"));
    REQUIRE_FALSE(hasMeshInputTarget("blue"));
}

TEST_CASE("Envelope coalesces the red and blue triple routes with yellow disabled",
        "[cycle-v2][modulation][triple][envelope][ui]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(
            NodeKind::ModulationTriple,
            "triple",
            { 10.f, 20.f }));
    graph.addNode(factory.createNode(
            NodeKind::Envelope,
            "envelope",
            { 420.f, 20.f }));
    const Node* triple = graph.findNode("triple");
    const Node* envelope = graph.findNode("envelope");
    REQUIRE(triple != nullptr);
    REQUIRE(envelope != nullptr);

    const PortAddress source = ModulationCableBundle::sourceAddress(*triple);
    const PortAddress destination = ModulationCableBundle::destinationAddress(*envelope);
    const auto routes = ModulationCableBundle::routes(graph, source, destination);
    REQUIRE(routes.size() == 2);
    REQUIRE(routes[0].source.portId == "red");
    REQUIRE(routes[0].destination.portId == "red");
    REQUIRE(routes[1].source.portId == "blue");
    REQUIRE(routes[1].destination.portId == "blue");
    REQUIRE(ModulationCableBundle::canConnect(graph, source, destination));
    for (const auto& route : routes) {
        REQUIRE(GraphEditor().connect(graph, route.source, route.destination).succeeded());
    }

    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1000.f, 700.f });
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().modulationBundle);
    REQUIRE(snapshot.edges.front().edgeIndices.size() == 2);
    REQUIRE_FALSE(snapshot.edges.front().destinationBundleIncludesYellow);

    const auto envelopeInputCount = std::count_if(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::InputPort
                        && target.nodeId == "envelope";
            });
    REQUIRE(envelopeInputCount == 1);
    REQUIRE(std::any_of(
            snapshot.targets.begin(),
            snapshot.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.nodeId == "envelope"
                        && target.portId == ModulationCableBundle::portId();
            }));
}

TEST_CASE("Modulation triple exposes the shared hosted editor",
        "[cycle-v2][modulation][triple][ui]") {
    REQUIRE(NodeEditorFactoryRegistry::instance().find(NodeKind::ModulationTriple) != nullptr);
    const auto& capabilities = NodeViewModuleRegistry::instance()
            .moduleFor(NodeKind::ModulationTriple)
            .capabilities();
    REQUIRE(capabilities.hostedEditor);
    REQUIRE_FALSE(capabilities.previewable);
}
