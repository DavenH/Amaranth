#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "UI/ExplanationEditor.h"
#include "UI/NodeCanvasPresentation.h"

using namespace CycleV2;
using namespace juce;

namespace {

NodeGraph explanationGraph(int unrelatedNodes = 0) {
    NodeGraph graph;
    GraphNodeFactory factory;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 300.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOutput", { 200.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalInput", { 250.f, 0.f }));
    graph.addEdge({ "wave", "out", "voiceOutput", "time", PortDomain::TimeSignal });
    graph.addEdge({ "globalInput", "time", "output", "time", PortDomain::TimeSignal });
    for (int index = 0; index < unrelatedNodes; ++index) {
        graph.addNode(factory.createNode(NodeKind::Envelope, "other" + String(index), {}));
    }
    return graph;
}

template<typename Control>
Control& editorControl(ExplanationEditor& editor, const String& id) {
    auto* control = dynamic_cast<Control*>(editor.findChildWithID(id));
    REQUIRE(control != nullptr);
    return *control;
}

}

TEST_CASE("Explanations round trip as authored presentation with ordered undo",
        "[cycle-v2][explanations][serialization][undo]") {
    GraphDocument document(explanationGraph());
    GraphCommandDispatcher commands(document);
    const auto synthesisRevision = document.graph().getRevision();
    const String rationale = "Transient body\nThe early harmonics decay faster. \"Rounded\" & warm.";
    REQUIRE(commands.setNodeRationale("wave", rationale).changed);
    REQUIRE(commands.setPresetDescription("A warm key sound\nTry low velocities.").changed);
    REQUIRE(document.graph().getRevision() == synthesisRevision);
    REQUIRE(document.lastChange().explanationsChanged);
    REQUIRE_FALSE(document.lastChange().modelChanged);
    REQUIRE_FALSE(document.lastChange().topologyChanged);
    REQUIRE(document.lastChange().parameterImpacts == ParameterImpact::None);
    REQUIRE_FALSE(commands.setNodeRationale("wave", rationale).changed);

    const File file = File(String(CYCLE_V2_TEST_OUTPUT_DIR)).getChildFile("explanation-roundtrip.cyclegraph");
    REQUIRE(document.save(file));
    REQUIRE_FALSE(document.isDirty());
    GraphDocument reloaded;
    REQUIRE(reloaded.load(file));
    REQUIRE(reloaded.graph().findNode("wave")->rationale == rationale);
    REQUIRE(reloaded.presentation().description == document.presentation().description);
    REQUIRE(reloaded.toJson() == document.toJson());

    REQUIRE(document.undo());
    REQUIRE(document.presentation().description.isEmpty());
    REQUIRE(document.graph().findNode("wave")->rationale == rationale);
    REQUIRE(document.isDirty());
    REQUIRE(document.redo());
    REQUIRE_FALSE(document.isDirty());
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("wave")->rationale.isEmpty());
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.redo());

    REQUIRE(commands.removeNode("wave").succeeded());
    REQUIRE(document.graph().findNode("wave") == nullptr);
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("wave")->rationale == rationale);
    Node copied = *document.graph().findNode("wave");
    copied.id = "copy";
    NodeGraph copiedGraph;
    copiedGraph.addNode(std::move(copied));
    REQUIRE(copiedGraph.findNode("copy")->rationale == rationale);
    REQUIRE(commands.setNodeRationale("wave", {}).changed);
    REQUIRE(document.graph().findNode("wave")->rationale.isEmpty());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("wave")->rationale == rationale);
}

TEST_CASE("Explanation validation rejects malformed and stale targets atomically",
        "[cycle-v2][explanations][validation]") {
    GraphDocument document(explanationGraph());
    GraphCommandDispatcher commands(document);
    const auto revision = document.revision();
    REQUIRE_FALSE(commands.setNodeRationale("missing", "text").succeeded());
    REQUIRE_FALSE(commands.setNodeRationale("wave", String::repeatedString("x", maximumExplanationLength + 1)).succeeded());
    REQUIRE_FALSE(commands.setPresetDescription(String::repeatedString("x", maximumExplanationLength + 1)).succeeded());
    REQUIRE(document.revision() == revision);
    REQUIRE_FALSE(document.canUndo());
    commands.beginTransientEdit();
    REQUIRE_FALSE(commands.setNodeRationale("wave", "during gesture").succeeded());
    commands.cancelTransientEdit();
    REQUIRE_FALSE(document.canUndo());

    var encoded = GraphSerializer().writeJSON(document.graph());
    encoded.getProperty("nodes", {})[0].getDynamicObject()->setProperty("rationale", 42);
    REQUIRE_FALSE(GraphSerializer().readJSON(encoded).succeeded());
    REQUIRE_FALSE(document.loadJson(JSON::toString(encoded)));
    REQUIRE(document.revision() == revision);
    auto* presentation = new DynamicObject();
    presentation->setProperty("version", 1);
    presentation->setProperty("description", 42);
    encoded = GraphSerializer().writeJSON(document.graph());
    encoded.getDynamicObject()->setProperty("presetPresentation", presentation);
    REQUIRE_FALSE(document.loadJson(JSON::toString(encoded)));
    REQUIRE(document.revision() == revision);
}

TEST_CASE("Explanation edits and history never copy unrelated synthesis state",
        "[cycle-v2][explanations][complexity]") {
    const int unrelatedNodes = GENERATE(0, 128);
    GraphDocument document(explanationGraph(unrelatedNodes));
    GraphCommandDispatcher commands(document);
    const auto synthesisRevision = document.graph().getRevision();
    InteractionComplexityDiagnostics::reset();
    REQUIRE(commands.setNodeRationale("wave", "A restrained body layer").changed);
    REQUIRE(commands.setPresetDescription("Warm and responsive").changed);
    REQUIRE(document.undo());
    REQUIRE(document.undo());
    REQUIRE(document.redo());
    REQUIRE(document.redo());
    const auto counts = InteractionComplexityDiagnostics::counts();
    REQUIRE(counts.graphCopies == 0);
    REQUIRE(counts.audioSamplesCopied == 0);
    REQUIRE(counts.meshCopies == 0);
    REQUIRE(counts.modelSerializations == 0);
    REQUIRE(counts.validationNodeVisits == 0);
    REQUIRE(counts.nodeLinearScans == 0);
    REQUIRE(document.graph().getRevision() == synthesisRevision);
}

TEST_CASE("Explanation editor keeps drafts local and commits one complete text edit",
        "[cycle-v2][explanations][ui]") {
    ScopedJuceInitialiser_GUI gui;
    GraphDocument document(explanationGraph());
    GraphCommandDispatcher commands(document);
    ExplanationEditor editor("Wave explanation", "Generates a waveform for the current voice.", {},
            [&](const String& text) { return commands.setNodeRationale("wave", text).succeeded(); });
    auto& text = editorControl<TextEditor>(editor, "explanation.text");
    const Label* typeHelp = nullptr;
    for (auto* child : editor.getChildren()) {
        if (const auto* label = dynamic_cast<const Label*>(child);
                label != nullptr && label->getText().contains("Generates a waveform")) {
            typeHelp = label;
        }
    }
    REQUIRE(typeHelp != nullptr);
    REQUIRE(typeHelp->getHeight() >= 28);
    REQUIRE(text.getY() >= typeHelp->getBottom() + 8);
    REQUIRE(text.getX() == editor.getWidth() - text.getRight());
    REQUIRE(editorControl<TextButton>(editor, "explanation.save").getBottom()
            == editorControl<TextButton>(editor, "explanation.cancel").getBottom());
    text.setText("First draft", false);
    text.setText("The bright attack fades\ninto a warm sustain.", false);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.graph().findNode("wave")->rationale.isEmpty());
    REQUIRE(editor.automationState().getProperty("typeHelp", {}).toString().contains("waveform"));
    editorControl<TextButton>(editor, "explanation.save").onClick();
    REQUIRE(document.graph().findNode("wave")->rationale == text.getText());
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.graph().findNode("wave")->rationale.isEmpty());
    text.setText("Cancelled", false);
    editorControl<TextButton>(editor, "explanation.cancel").onClick();
    REQUIRE_FALSE(document.canUndo());
    text.onEscapeKey();
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Document replacement history keeps each preset explanation with its graph",
        "[cycle-v2][explanations][replacement][undo]") {
    GraphDocument document(explanationGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeRationale("wave", "Body A").changed);
    REQUIRE(commands.setPresetDescription("Instrument A").changed);
    const File directory(String(CYCLE_V2_TEST_OUTPUT_DIR));
    const File fileA = directory.getChildFile("explanation-A.cyclegraph");
    const File fileB = directory.getChildFile("explanation-B.cyclegraph");
    const File fileSaved = directory.getChildFile("explanation-history-saved.cyclegraph");
    REQUIRE(document.save(fileA));
    REQUIRE(commands.setNodeRationale("wave", "Body B").changed);
    REQUIRE(commands.setPresetDescription("Instrument B").changed);
    REQUIRE(document.save(fileB));
    REQUIRE(document.load(fileA));
    REQUIRE(document.presentation().description == "Instrument A");
    REQUIRE(document.load(fileB));
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("wave")->rationale == "Body A");
    REQUIRE(document.presentation().description == "Instrument A");
    REQUIRE(document.save(fileSaved));
    GraphDocument loaded;
    REQUIRE(loaded.load(fileSaved));
    REQUIRE(loaded.presentation().description == "Instrument A");
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNode("wave")->rationale == "Body B");
    REQUIRE(document.presentation().description == "Instrument B");
    REQUIRE(document.save(fileSaved));
    REQUIRE(loaded.load(fileSaved));
    REQUIRE(loaded.presentation().description == "Instrument B");
}

TEST_CASE("Information actions stay inside headers and clear existing controls",
        "[cycle-v2][explanations][ui][geometry]") {
    GraphNodeFactory factory;
    for (const auto kind : { NodeKind::TrilinearMesh, NodeKind::Envelope, NodeKind::Reverb }) {
        const Node node = factory.createNode(kind, "node", {});
        const bool global = kind == NodeKind::Reverb;
        for (const float zoom : { 0.43f, 1.f, 1.5f }) {
            const Rectangle<float> bounds(100.f, 80.f, node.bounds.getWidth() * zoom,
                    node.bounds.getHeight() * zoom);
            const auto action = ExplanationBadge::boundsFor(node, bounds, zoom, global);
            const auto header = bounds.withHeight(42.f * zoom);
            REQUIRE(header.contains(action));
            REQUIRE(action.getCentreY() == header.getCentreY());
            const auto layoutControl = Rectangle<float>(27.f * zoom, 27.f * zoom).withCentre(
                    { bounds.getRight() - 21.f * zoom, header.getCentreY() });
            REQUIRE_FALSE(action.intersects(layoutControl));
            const float titleRight = bounds.getRight() - (13.f
                    + ExplanationBadge::titleReservation(node, global)) * zoom;
            REQUIRE(titleRight < action.getX());
            if (global) {
                REQUIRE_FALSE(action.intersects(NodeCanvasPresentation::globalProcessingIndicatorBounds(
                        header, zoom, true)));
            }
        }
    }
}
