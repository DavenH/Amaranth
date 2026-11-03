#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <UI/Panels/TimeSurfaceStyles.h>

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
#include "UI/SpyFrameRenderer.h"
#include "UI/SpyPlaybackRipple.h"
#include "UI/SignalProbeDetailView.h"
#include "App/Automation/SpyFrameAutomation.h"

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

TEST_CASE("Controller batches refresh shared roots once without unrelated graph work",
        "[cycle-v2][spy-export][preview-controller][complexity]") {
    ScopedJuceInitialiser_GUI juce;
    for (const int unrelated : { 1, 80 }) {
        auto graph = controllerGraph(unrelated);
        graph.addNode(GraphNodeFactory().createNode(NodeKind::ModulationSource, "second", {}));
        GraphNodeStateEditor state;
        REQUIRE(state.setNodeParameter(graph, "second", "source", "Source", "midiCC").succeeded());
        REQUIRE(state.setNodeParameter(graph, "second", "controller", "Controller", "74").succeeded());
        graph.addEdge({ "second", "value", "mesh", "yellow", PortDomain::ControlSignal, ConnectionKind::Signal });
        GraphDocument document(std::move(graph));
        GraphPresentationModel presentation;
        REQUIRE(presentation.refresh(document.graph(), document.revision()));
        const auto revision = document.revision();
        const auto audioRevision = presentation.audioPlanRevision();
        const auto compilations = presentation.compilationCount();
        const auto renders = presentation.previewRenderCount();
        const auto unrelatedProcesses = presentation.previewAudioProcessCount("unrelated0");
        auto values = presentation.previewControllerValues();
        values[73] = 32;
        values[74] = 96;
        values[125] = 80;
        InteractionComplexityDiagnostics::reset();
        REQUIRE(presentation.refreshPreviewControllerValues(document.graph(), revision, values));
        const auto counts = InteractionComplexityDiagnostics::counts();
        CHECK(counts.graphCopies == 0);
        CHECK(counts.meshCopies == 0);
        CHECK(counts.modelSerializations == 0);
        CHECK(presentation.previewRenderCount() == renders + 1);
        CHECK(presentation.previewAudioProcessCount("unrelated0") == unrelatedProcesses);
        CHECK(presentation.audioPlanRevision() == audioRevision);
        CHECK(presentation.compilationCount() == compilations);
        CHECK(document.revision() == revision);
        REQUIRE(presentation.refreshPreviewControllerValues(document.graph(), revision, values));
        CHECK(presentation.previewRenderCount() == renders + 1);
        values[73] = 128;
        CHECK_FALSE(presentation.refreshPreviewControllerValues(document.graph(), revision, values));
        CHECK(presentation.previewControllerValue(73) == 32);
    }
}

namespace {

bool equalPixels(const Image& first, const Image& second) {
    if (first.getBounds() != second.getBounds()) {
        return false;
    }
    for (int y = 0; y < first.getHeight(); ++y) {
        for (int x = 0; x < first.getWidth(); ++x) {
            if (first.getPixelAt(x, y) != second.getPixelAt(x, y)) {
                return false;
            }
        }
    }
    return true;
}

var spyRequest(const String& gradient) {
    auto object = std::make_unique<DynamicObject>();
    Array<var> values;
    values.insertMultiple(0, var(0), 128);
    object->setProperty("controllers", values);
    object->setProperty("gradient", gradient);
    return var(object.release());
}

}

TEST_CASE("Spy export uses expanded capture and built-in materials without document changes",
        "[cycle-v2][spy-export][surface-program]") {
    ScopedJuceInitialiser_GUI juce;
    NodeGraph graph = controllerGraph(0);
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 1, 0.5f).succeeded());
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeParameter("mesh", "yellow", "Yellow", "0.2").changed);
    REQUIRE(document.undo());
    REQUIRE(document.canRedo());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(document.graph(), document.revision()));
    PresetPresentation metadata;
    metadata.timeSurfaceStyle = "bullion";
    const String authored = document.graph().getSignalProbes().front().id;
    presentation.setProbeNoteOffTimes({ { authored, 0.4 }, { "default-output", 0.4 } });
    const auto before = GraphSerializer().toJsonString(document.graph());
    const auto audioRevision = presentation.audioPlanRevision();
    const auto compilations = presentation.compilationCount();
    const auto globalStyle = ScalarSurfaceMaterial::timeSurfaceStyle();
    String error;
    SpyFrameRequest request;
    request.width = 160;
    request.height = 96;
    request.controllers[73] = 32;

    InteractionComplexityDiagnostics::reset();
    for (const auto& id : { String("default-output"), authored }) {
        request.probeId = id;
        const auto inherited = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
        REQUIRE(error.isEmpty());
        REQUIRE(inherited.image.isValid());
        CHECK(inherited.image.getPixelAt(0, 0).isOpaque());
        CHECK(inherited.image.getWidth() == request.width);
        CHECK(inherited.image.getHeight() == request.height);
        CHECK(inherited.gradient == ScalarSurfaceTimeStyle::Bullion);
        CHECK(inherited.noteOffTime == Catch::Approx(0.4));
        for (const auto style : { ScalarSurfaceTimeStyle::Bullion,
                    ScalarSurfaceTimeStyle::Greyscale, ScalarSurfaceTimeStyle::BlueDepth }) {
            request.gradient = style;
            const auto actual = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
            REQUIRE(error.isEmpty());
            SignalProbeDetailState detail;
            REQUIRE(detail.capture(document.graph(), presentation, id, PresetPreviewView::Time, 0.4, 0));
            const auto profile = TrimeshRenderProfile::fromSemantic({ detail.domain, detail.scalePolicy,
                    RenderSemanticRole::Generic }).withTimeStyle(style);
            const auto heat = NodePreviewRenderer::createRuntimeHeatmapImage(detail.renderResult, profile);
            Image expected(Image::RGB, request.width, request.height, true);
            Graphics graphics(expected);
            graphics.setImageResamplingQuality(Graphics::highResamplingQuality);
            graphics.drawImage(heat, expected.getBounds().toFloat());
            CHECK(equalPixels(actual.image, expected));
            CHECK(actual.contentRevision == inherited.contentRevision);
            CHECK(profile.getSurfaceStyle().materialStyleSignature() == (int) style);
            if (style == ScalarSurfaceTimeStyle::Bullion) {
                CHECK(equalPixels(actual.image, inherited.image));
            } else {
                CHECK_FALSE(equalPixels(actual.image, inherited.image));
            }
            const auto repeated = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
            CHECK(equalPixels(actual.image, repeated.image));
            request.playback.count = 2;
            request.playback.voices[0] = { 1, 60, 0.2, 0., 1., false };
            request.playback.voices[1] = { 2, 67, 0.5, 0., 1., false };
            const auto lit = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
            REQUIRE(lit.image.isValid());
            CHECK(lit.contentRevision == actual.contentRevision);
            CHECK(lit.playbackPositions.size() == 2);
            auto expectedRipples = actual.image.createCopy();
            SpyPlaybackRipple::apply(expectedRipples, lit.playbackPositions);
            CHECK(equalPixels(lit.image, expectedRipples));
            request.playback.count = 0;
        }
        request.gradient.reset();
    }
    metadata.timeSurfaceStyle.clear();
    const auto fallback = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
    CHECK(fallback.gradient == TimeSurfaceStyles::defaultStyle);
    request.midiNote = 72;
    const auto selectedNote = SpyFrameRenderer::render(document.graph(), presentation, metadata, request, error);
    REQUIRE(error.isEmpty());
    CHECK(selectedNote.midiNote == 72);
    const auto copies = InteractionComplexityDiagnostics::counts();
    CHECK(copies.graphCopies == 0);
    CHECK(copies.meshCopies == 0);
    CHECK(copies.modelSerializations == 0);
    CHECK(ScalarSurfaceMaterial::timeSurfaceStyle() == globalStyle);
    CHECK(GraphSerializer().toJsonString(document.graph()) == before);
    CHECK_FALSE(document.canUndo());
    CHECK(document.canRedo());
    CHECK(presentation.audioPlanRevision() == audioRevision);
    CHECK(presentation.compilationCount() == compilations);
}

TEST_CASE("Spy frame protocol lists gradients and rejects malformed requests and invalid targets",
        "[cycle-v2][spy-export][automation]") {
    ScopedJuceInitialiser_GUI juce;
    String error;
    const auto listed = SpyFrameAutomation::gradients();
    const auto entries = listed["data"];
    REQUIRE(entries.isArray());
    CHECK(entries.getArray()->size() == (int) TimeSurfaceStyles::entries.size());
    for (const auto& entry : TimeSurfaceStyles::entries) {
        for (const auto& name : { String(entry.id).toUpperCase(), String(entry.label).toUpperCase() }) {
            const auto decoded = SpyFrameAutomation::readRequest(spyRequest(name), error);
            REQUIRE(decoded.has_value());
            CHECK(decoded->gradient == entry.style);
        }
    }
    CHECK_FALSE(SpyFrameAutomation::readRequest(spyRequest("missing"), error).has_value());
    CHECK(error.contains("bullion"));
    auto invalid = spyRequest("bullion");
    invalid.getDynamicObject()->setProperty("midiNote", 128);
    CHECK_FALSE(SpyFrameAutomation::readRequest(invalid, error).has_value());
    invalid.getDynamicObject()->removeProperty("midiNote");
    invalid.getDynamicObject()->setProperty("width", 0);
    CHECK_FALSE(SpyFrameAutomation::readRequest(invalid, error).has_value());
    invalid.getDynamicObject()->removeProperty("width");
    invalid.getDynamicObject()->setProperty("controllers", Array<var> { 0 });
    CHECK_FALSE(SpyFrameAutomation::readRequest(invalid, error).has_value());

    auto graph = controllerGraph(0);
    graph.removeEdgeAt(1);
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Fft, "fft", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Ifft, "ifft", {}));
    graph.addEdge({ "mesh", "out", "fft", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, ConnectionKind::Signal });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, ConnectionKind::Signal });
    graph.addEdge({ "ifft", "time", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 2, 0.5f).succeeded());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, 1));
    SpyFrameRequest request;
    request.probeId = "missing";
    CHECK_FALSE(SpyFrameRenderer::render(graph, presentation, {}, request, error).image.isValid());
    request.probeId = graph.getSignalProbes().front().id;
    CHECK_FALSE(SpyFrameRenderer::render(graph, presentation, {}, request, error).image.isValid());
    CHECK(error.contains("time-signal"));
    request.probeId = "default-output";
    request.gradient = ScalarSurfaceTimeStyle::Bullion;
    request.width = 80;
    request.height = 48;
    const auto frame = SpyFrameRenderer::render(graph, presentation, {}, request, error);
    REQUIRE(frame.image.isValid());
    TemporaryFile target(".png");
    REQUIRE((bool) SpyFrameAutomation::writeFrame(frame, target.getFile())["ok"]);
    CHECK(equalPixels(frame.image, ImageFileFormat::loadFrom(target.getFile())));
    CHECK_FALSE((bool) SpyFrameAutomation::writeFrame(frame, target.getFile())["ok"]);
    const File blocked = target.getFile().getChildFile("frame.png");
    CHECK_FALSE((bool) SpyFrameAutomation::writeFrame(frame, blocked)["ok"]);
    request.probeId = graph.getSignalProbes().front().id;
    graph.removeNode("fft");
    REQUIRE(presentation.refresh(graph, graph.getRevision()));
    CHECK_FALSE(SpyFrameRenderer::render(graph, presentation, {}, request, error).image.isValid());
}

TEST_CASE("Spy frame protocol validates bounded playback ages",
        "[cycle-v2][spy-movie][automation]") {
    String error;
    auto command = spyRequest("bullion");
    var voice = JSON::parse(R"({"ageSeconds":0.2,"releaseAgeSeconds":0,"durationSeconds":1,"released":false})");
    auto* object = command.getDynamicObject();
    object->setProperty("playbackVoices", Array<var> { voice });
    REQUIRE(SpyFrameAutomation::readRequest(command, error).has_value());
    for (const double invalid : { -1., 0. }) {
        voice.getDynamicObject()->setProperty("durationSeconds", invalid);
        CHECK_FALSE(SpyFrameAutomation::readRequest(command, error).has_value());
    }
    voice.getDynamicObject()->setProperty("durationSeconds", 1.);
    voice.getDynamicObject()->setProperty("ageSeconds", -0.1);
    CHECK_FALSE(SpyFrameAutomation::readRequest(command, error).has_value());
    voice.getDynamicObject()->setProperty("ageSeconds", 0.1);
    Array<var> excessive;
    excessive.insertMultiple(0, voice, 9);
    object->setProperty("playbackVoices", excessive);
    CHECK_FALSE(SpyFrameAutomation::readRequest(command, error).has_value());
}
