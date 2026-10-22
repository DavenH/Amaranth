#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "NodeGraphTestAccess.h"
#include "EnvelopeAuthoringTestSupport.h"

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphSerializer.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/MeshDimensionality.h"
#include "Graph/NodeParameterMap.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/Editor/EnvelopePanelAdapter.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Runtime/GraphPresentationModel.h"
#include "Runtime/RealtimeGraphRenderer.h"
#include "UI/PerformanceKeyboard.h"

using namespace CycleV2;

namespace {

NodeGraph controllerGraph(int unrelated) {
    GraphNodeFactory factory;
    GraphNodeStateEditor state;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "cc", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    REQUIRE(state.setNodeParameter(graph, "cc", "source", "Source", "midiCC").succeeded());
    REQUIRE(state.setNodeParameter(graph, "cc", "controller", "Controller", "73").succeeded());
    REQUIRE(state.setNodeParameter(graph, "mesh", "dimensionCount", "Dimensions", "3").succeeded());
    graph.addEdge({ "cc", "value", "mesh", "red", PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "mesh", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    for (int index = 0; index < unrelated; ++index) {
        const String id = "unrelated" + String(index);
        graph.addNode(factory.createNode(NodeKind::ModulationSource, id, {}));
        REQUIRE(state.setNodeParameter(graph, id, "source", "Source", "constant").succeeded());
    }
    return graph;
}

class ControllerSink final : public MidiEventSink {
public:
    bool enqueueMidiMessage(const MidiMessage& message, MidiEventSource) override {
        messages.push_back(message);
        return true;
    }
    bool enqueueMidiMessageAt(const MidiMessage& message, MidiEventSource source, double) override {
        return enqueueMidiMessage(message, source);
    }
    void releaseMidiSource(MidiEventSource) override {}

    std::vector<MidiMessage> messages;
};

}

TEST_CASE("Audition controllers preserve document history and scope preview work",
        "[cycle-v2][preview-controller][preview-cc][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    for (const int unrelated : { 1, 80 }) {
        GraphDocument document(controllerGraph(unrelated));
        GraphCommandDispatcher commands(document);
        REQUIRE(commands.setNodeParameter("mesh", "yellow", "Yellow", "0.2").changed);
        REQUIRE(document.undo());
        REQUIRE(document.canRedo());
        GraphPresentationModel presentation;
        REQUIRE(presentation.refresh(document.graph(), document.revision()));
        const String before = GraphSerializer().toJsonString(document.graph());
        const auto revision = document.revision();
        const auto audioRevision = presentation.audioPlanRevision();
        const auto compilations = presentation.compilationCount();
        const auto unrelatedProcesses = presentation.previewAudioProcessCount("unrelated0");
        const auto model = document.graph().findNode("mesh")->model;
        InteractionComplexityDiagnostics::reset();
        REQUIRE(presentation.refreshPreviewControllerValue(document.graph(), revision, 73, 32));
        REQUIRE(presentation.refreshPreviewControllerValue(document.graph(), revision, 73, 96));
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        for (const int controller : { 1, 2, 3, 5, 76, 125 }) {
            REQUIRE(presentation.refreshPreviewControllerValue(document.graph(), revision, controller, controller));
            REQUIRE(presentation.refreshPreviewControllerValue(document.graph(), revision, controller, controller));
            REQUIRE(presentation.previewControllerValue(controller) == controller);
        }
        const auto effective = presentation.previewMorphForNode(*document.graph().findNode("mesh"));
        REQUIRE(effective.has_value());
        REQUIRE((float) effective->red == Catch::Approx(96.f / 127.f));
        REQUIRE(presentation.previewControllerValues().size() == 128);
        REQUIRE(document.revision() == revision);
        REQUIRE_FALSE(document.canUndo());
        REQUIRE(document.canRedo());
        REQUIRE(document.graph().findNode("mesh")->model == model);
        REQUIRE(GraphSerializer().toJsonString(document.graph()) == before);
        REQUIRE(presentation.audioPlanRevision() == audioRevision);
        REQUIRE(presentation.compilationCount() == compilations);
        REQUIRE(presentation.previewAudioProcessCount("unrelated0") == unrelatedProcesses);
    }
}

TEST_CASE("Envelope-only default controllers refresh downstream spies through effective routes",
        "[cycle-v2][preview-controller][preview-cc][envelope][probe][mapping]") {
    ScopedJuceInitialiser_GUI juce;
    TestSupport::EnvelopeAuthoringCurveTables tables;
    GraphNodeFactory factory;
    GraphNodeStateEditor state;
    for (const bool explicitOverride : { false, true }) {
        NodeGraph graph = TestSupport::authoredEnvelopeGraph(2);
        graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
        graph.addNode(factory.createNode(NodeKind::ModulationTriple, "morph", {}));
        graph.addNode(factory.createNode(NodeKind::ModulationSource, "constant", {}));
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        REQUIRE(state.setNodeParameter(graph, "constant", "source", "Source", "constant").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "blueSource", "Blue Source", "constant").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "envelopeYellowSource", "Envelope Yellow Source", "midiCC").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "envelopeYellowController", "Envelope Yellow Controller", "77").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "envelopeRedSource", "Envelope Red Source", "midiCC").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "envelopeRedController", "Envelope Red Controller", "76").succeeded());
        REQUIRE(state.setNodeParameter(graph, "morph", "envelopeBlueSource", "Envelope Blue Source", "constant").succeeded());
        auto mesh = TrimeshMeshFactory::createDefaultMesh("EnvelopeControllerSpy");
        REQUIRE(graph.replaceNodeModel("mesh", TrimeshNodeModelState::takeOwnership(std::move(mesh), 1)));
        GraphEditor editor;
        REQUIRE(editor.connect(graph, { "morph", "modulation", false }, { "voice", "modulation", true }).succeeded());
        REQUIRE(editor.connect(graph, { "mesh", "out", false }, { "multiply", "left", true }).succeeded());
        REQUIRE(editor.connect(graph, { "env", "env", false }, { "multiply", "right", true }).succeeded());
        REQUIRE(editor.connect(graph, { "multiply", "out", false }, { "out", "time", true }).succeeded());
        REQUIRE(editor.toggleSignalProbe(graph, graph.getEdges().size() - 1, .5f).succeeded());
        if (explicitOverride) {
            REQUIRE(editor.connect(graph, { "constant", "value", false }, { "env", "red", true }).succeeded());
        }
        GraphPresentationModel presentation;
        REQUIRE(presentation.refresh(graph, 1));
        REQUIRE(presentation.hasControllerPreviewRoots(76) == !explicitOverride);
        REQUIRE_FALSE(presentation.hasControllerPreviewRoots(77));
        REQUIRE_FALSE(presentation.hasControllerPreviewRoots(1));
        const auto& targets = presentation.controllerMorphTargets(76);
        REQUIRE(targets.size() == (explicitOverride ? 0 : 1));
        if (!explicitOverride) {
            REQUIRE(targets.front().nodeId == "env");
            REQUIRE(targets.front().parameterId == "red");
            const auto& buffers = presentation.compileResult().plan.buffers;
            REQUIRE(std::any_of(buffers.begin(), buffers.end(), [](const auto& buffer) {
                return buffer.sourceNodeId == "voice"
                        && buffer.defaultModulationSlot == DefaultModulationSlot::Red
                        && buffer.envelopeModulationProfile;
            }));
        }
        const auto spyValues = [&]() {
            const auto& probes = presentation.previewResult().probes;
            REQUIRE(probes.size() == 1);
            return probes.front().values;
        };
        const auto compilationCount = presentation.compilationCount();
        const auto audioRevision = presentation.audioPlanRevision();
        const auto unrelatedProcesses = presentation.previewAudioProcessCount("constant");
        const auto meshProcesses = presentation.previewAudioProcessCount("mesh");
        REQUIRE(presentation.refreshPreviewControllerValue(graph, 1, 76, 16));
        const auto lowerSpy = spyValues();
        REQUIRE_FALSE(lowerSpy.empty());
        REQUIRE(presentation.refreshPreviewControllerValue(graph, 1, 76, 112));
        const auto higherSpy = spyValues();
        CHECK((higherSpy != lowerSpy) == !explicitOverride);
        const auto renders = presentation.previewRenderCount();
        REQUIRE(presentation.refreshPreviewControllerValue(graph, 1, 77, 100));
        CHECK(presentation.previewRenderCount() == renders);
        CHECK(presentation.compilationCount() == compilationCount);
        CHECK(presentation.audioPlanRevision() == audioRevision);
        CHECK(presentation.previewAudioProcessCount("constant") == unrelatedProcesses);
        CHECK(presentation.previewAudioProcessCount("mesh") == meshProcesses);
    }
}

TEST_CASE("Effective mesh previews leave authored morph and later publications intact",
        "[cycle-v2][preview-controller][mesh][envelope][publication]") {
    ScopedJuceInitialiser_GUI juce;
    GraphNodeFactory factory;
    GraphNodeStateEditor state;
    for (const int dimensions : { 0, 1, 2, 3 }) {
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
        REQUIRE(state.setNodeParameter(graph, "env", "dimensionCount", "Dimensions", String(dimensions)).succeeded());
        const Node& node = *graph.findNode("env");
        const auto authored = MeshDimensionality::position(node);
        const MorphPosition preview { .9f, .8f, .7f };
        EnvelopePanelAdapter adapter;
        REQUIRE(adapter.syncFromNode(node));
        adapter.setPreviewMorphPosition(preview);
        const auto effective = adapter.effectiveMorphPosition();
        for (const int axis : MorphDimensions::axes) {
            REQUIRE((float) effective[axis] == Catch::Approx((float)
                    (MorphDimensions(dimensions).active(axis) ? preview[axis] : authored[axis])));
        }
        REQUIRE_FALSE(adapter.setPreviewMorphPosition(preview));
        REQUIRE_FALSE(adapter.mesh().getVerts().empty());
        adapter.mesh().getVerts().front()->values[Vertex::Amp] = .35f;
        const auto publication = std::dynamic_pointer_cast<const CurveNodeModelState>(
                adapter.modelPublication(adapter.selectedMeshCube(), node.model->revision() + 1));
        REQUIRE(publication != nullptr);
        REQUIRE(publication->envelopeRed() == (float) authored.red);
        REQUIRE(publication->envelopeBlue() == (float) authored.blue);
        REQUIRE(publication->envelope()->getMesh().getVerts().front()->values[Vertex::Amp] == .35f);
        adapter.setPreviewMorphPosition(std::nullopt);
        REQUIRE((float) adapter.effectiveMorphPosition().red == (float) authored.red);
    }

    const NodeGraph graph = controllerGraph(0);
    const Node& node = *graph.findNode("mesh");
    const float authoredRed = NodeParameterMap(node).floatValue("red");
    TrimeshNodeModel model;
    model.syncFromNode(node);
    const MorphPosition preview { .9f, .8f, .7f };
    InteractionComplexityDiagnostics::reset();
    REQUIRE_FALSE(model.syncFromNode(node, TrimeshSelectionSyncPolicy::PreserveCurrent, preview));
    const auto revision = model.getRevision();
    REQUIRE_FALSE(model.syncFromNode(node, TrimeshSelectionSyncPolicy::PreserveCurrent, preview));
    REQUIRE(model.getRevision() == revision);
    REQUIRE((float) model.getMorphPosition().red == .8f);
    REQUIRE(NodeParameterMap(node).floatValue("red") == authoredRed);
    REQUIRE(InteractionComplexityDiagnostics::counts().meshCopies == 0);
}

TEST_CASE("Hidden controller wheels reflect one canonical state and playback uses it",
        "[cycle-v2][preview-controller][keyboard][midi-cc]") {
    ScopedJuceInitialiser_GUI juce;
    MidiKeyboardState keyboardState;
    ControllerSink sink;
    PerformanceKeyboardPanel panel(keyboardState, sink);
    std::array<int, 128> values {};
    int callbacks = 0;
    panel.setControllerValueProvider([&](int controller) { return values[(size_t) controller]; });
    panel.setControllerValueChangedCallback([&](int controller, int value) {
        ++callbacks;
        values[(size_t) controller] = value;
        REQUIRE(sink.enqueueMidiMessage(MidiMessage::controllerEvent(1, controller, value),
                MidiEventSource::PerformanceKeyboard));
        panel.reflectControllerValue(controller, value);
    });
    panel.setVisible(false);
    panel.setMappedControllers({ 1, 2, 3, 5, 76 });
    for (const int controller : { 1, 2, 3, 5, 76, 125 }) {
        values[(size_t) controller] = 100 - controller / 2;
        panel.reflectControllerValue(controller, values[(size_t) controller]);
        REQUIRE(panel.controllerValue(controller) == values[(size_t) controller]);
    }
    REQUIRE(callbacks == 0);
    REQUIRE(sink.messages.empty());
    panel.setModWheelValue(127);
    REQUIRE(callbacks == 1);
    REQUIRE(sink.messages.size() == 1);
    REQUIRE(sink.messages.front().isController());
    REQUIRE(sink.messages.front().getControllerValue() == 127);
    panel.setMappedControllers({ 1, 125 });
    values[125] = 37;
    sink.messages.clear();
    REQUIRE(panel.startPlayback(1000.0));
    REQUIRE(sink.messages.size() == 3);
    REQUIRE(sink.messages[0].getControllerNumber() == 1);
    REQUIRE(sink.messages[0].getControllerValue() == 127);
    REQUIRE(sink.messages[1].getControllerNumber() == 125);
    REQUIRE(sink.messages[1].getControllerValue() == 37);
    REQUIRE(sink.messages[2].isNoteOn());
    panel.stopPlayback();
}

TEST_CASE("Stored Envelope Yellow controller morph remains a deliberate undoable edit",
        "[cycle-v2][preview-controller][envelope][mapping][undo]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, "env", "dimensionCount", "Dimensions", "3").succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const float before = NodeParameterMap(*document.graph().findNode("env")).floatValue("yellow");
    std::array<float, 128> values {};
    const std::vector<PreviewMorphTarget> targets { { "env", "yellow", PreviewMorphControl::MidiController, 76 } };
    commands.beginTransientEdit();
    values[76] = .3f;
    REQUIRE(commands.setMappedPreviewMorph(targets, 0.f, values).changed);
    values[76] = .8f;
    REQUIRE(commands.setMappedPreviewMorph(targets, 0.f, values).changed);
    commands.commitTransientEdit();
    REQUIRE(NodeParameterMap(*document.graph().findNode("env")).floatValue("yellow") == Catch::Approx(.8f));
    REQUIRE(document.undo());
    REQUIRE(NodeParameterMap(*document.graph().findNode("env")).floatValue("yellow") == before);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("A live controller updates held audio without adding a voice",
        "[cycle-v2][preview-controller][realtime][held-note]") {
    GraphNodeFactory factory;
    GraphNodeStateEditor state;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "cc", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    REQUIRE(state.setNodeParameter(graph, "cc", "source", "Source", "midiCC").succeeded());
    REQUIRE(state.setNodeParameter(graph, "cc", "controller", "Controller", "73").succeeded());
    graph.addEdge({ "cc", "value", "out", "time", PortDomain::ControlSignal, ConnectionKind::Signal });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    AudioExecutionSpec spec;
    spec.maximumFrameCount = 64;
    spec.sampleRate = 48000.;
    auto prepared = RealtimeGraphRenderer::prepareGraph(compiled.plan, 1, spec);
    RealtimeGraphRenderer renderer;
    renderer.setPreparedGraph(prepared.get());
    RealtimeMidiEventQueue queue;
    REQUIRE(queue.enqueue(MidiMessage::noteOn(1, 62, (uint8) 100), MidiEventSource::Hardware, 1.0));
    AudioBuffer<float> output(2, 64);
    float* channels[] { output.getWritePointer(0), output.getWritePointer(1) };
    renderer.process(queue, channels, 2, 64, 48000., 1.0);
    const float before = output.getSample(0, 63);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);
    REQUIRE(queue.enqueue(MidiMessage::controllerEvent(1, 73, 127), MidiEventSource::Hardware, 1.0 + 64. / 48000.));
    renderer.process(queue, channels, 2, 64, 48000., 1.0 + 64. / 48000.);
    REQUIRE(renderer.diagnostics(queue).activeVoiceCount == 1);
    REQUIRE(output.getSample(0, 63) > before + .01f);
}
