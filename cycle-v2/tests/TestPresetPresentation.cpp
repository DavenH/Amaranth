#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <UI/Panels/TimeSurfaceStyles.h>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Graph/PresetPresentation.h"

using namespace CycleV2;
using namespace juce;

namespace {

PresetPreviewImage jpegPreview() {
    Image image(Image::RGB, 16, 9, true);
    Graphics graphics(image);
    graphics.fillAll(Colour(0xff16212b));
    graphics.setColour(Colour(0xff43c7d0));
    graphics.drawLine(0.f, 8.f, 15.f, 1.f, 2.f);

    MemoryOutputStream encoded;
    JPEGImageFormat().writeImageToStream(image, encoded);
    return { encoded.getMemoryBlock(), 16, 9, PresetPreviewView::Spectrum };
}

NodeGraph graphWithOutput() {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "output", {}));
    return graph;
}

}

TEST_CASE("Preset presentation round trips outside graph state",
        "[cycle-v2][preset][presentation][serialization]") {
    PresetPresentation presentation;
    presentation.author = "Daven";
    presentation.pack = "Factory";
    presentation.description = "A bright moving sound";
    presentation.tags = { "acid", "lead" };
    presentation.rating = 4;
    presentation.preview = jpegPreview();
    PresetMidiSequence sequence;
    sequence.durationSeconds = 4.0;
    sequence.notes.push_back({ 60, 96, 0.0, 1.25 });
    sequence.notes.push_back({ 67, 80, 1.5, 0.5 });
    sequence.controls.push_back({ 1, 92, 1.0 });
    presentation.sequence = sequence;

    const String json = GraphSerializer().toJsonString(graphWithOutput(), presentation);
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(json);

    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.presentation.author == "Daven");
    REQUIRE(loaded.presentation.pack == "Factory");
    REQUIRE(loaded.presentation.tags == StringArray { "acid", "lead" });
    REQUIRE(loaded.presentation.rating == 4);
    REQUIRE(loaded.presentation.preview.has_value());
    REQUIRE(loaded.presentation.preview->jpegData == presentation.preview->jpegData);
    REQUIRE(loaded.presentation.sequence.has_value());
    REQUIRE(loaded.presentation.sequence->notes.size() == 2);
    REQUIRE(loaded.presentation.sequence->notes[0].durationSeconds == 1.25);
    REQUIRE(loaded.presentation.sequence->controls.size() == 1);
    REQUIRE(loaded.presentation.sequence->controls[0].value == 92);
}

TEST_CASE("Preset MIDI changes are dirty presentation edits without graph publication",
        "[cycle-v2][preset][sequence]") {
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto graphRevision = document.revision();
    PresetMidiSequence phrase;
    phrase.notes.push_back({ 36, 100, 0.0, 0.4 });
    REQUIRE(commands.setPresetSequence(phrase));
    REQUIRE(document.isDirty());
    REQUIRE(document.revision() == graphRevision);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.presentation().sequence->notes[0].pitch == 36);
}

TEST_CASE("Comparison Spy pins follow preset presentation without graph edits",
        "[cycle-v2][preset][comparison]") {
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto graphRevision = document.revision();

    REQUIRE(commands.pinComparisonSpy("default-output"));
    REQUIRE_FALSE(commands.pinComparisonSpy("default-output"));
    REQUIRE_FALSE(commands.pinComparisonSpy("missing-probe"));
    REQUIRE(document.presentation().comparisonSpyPins
            == std::vector<String> { "default-output" });
    REQUIRE(document.revision() == graphRevision);
    REQUIRE(document.isDirty());
    REQUIRE_FALSE(document.canUndo());

    const auto saved = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(saved.succeeded());
    REQUIRE(saved.presentation.comparisonSpyPins
            == std::vector<String> { "default-output" });
    REQUIRE(commands.unpinComparisonSpy("default-output"));
    REQUIRE_FALSE(commands.unpinComparisonSpy("default-output"));
    REQUIRE(document.presentation().comparisonSpyPins.empty());
}

TEST_CASE("Ordered Spy pins switch with presets and retain disconnected probes",
        "[cycle-v2][preset][comparison]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({ "wave", "out", "output", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    const String probeId = graph.getSignalProbes().front().id;
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const String unpinnedPreset = document.toJson();

    REQUIRE(commands.pinComparisonSpy(probeId));
    REQUIRE(commands.pinComparisonSpy("default-output"));
    const String pinnedPreset = document.toJson();
    REQUIRE(document.presentation().comparisonSpyPins
            == std::vector<String> { probeId, "default-output" });

    REQUIRE(document.loadJson(unpinnedPreset, false));
    REQUIRE(document.presentation().comparisonSpyPins.empty());
    REQUIRE(document.loadJson(pinnedPreset, false));
    REQUIRE(document.presentation().comparisonSpyPins
            == std::vector<String> { probeId, "default-output" });
    REQUIRE(commands.removeEdgeAt(0).succeeded());
    REQUIRE(document.presentation().comparisonSpyPins
            == std::vector<String> { probeId, "default-output" });
    REQUIRE(document.graph().findSignalProbe(probeId) != nullptr);
}

TEST_CASE("Connected Trimesh output creates one graph probe before it is pinned",
        "[cycle-v2][preset][comparison]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addEdge({ "mesh", "out", "add", "right",
            PortDomain::ControlSignal, ConnectionKind::Signal });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const auto beforeProbe = document.revision();

    REQUIRE(commands.toggleSignalProbe(0, 0.5f).succeeded());
    REQUIRE(document.graph().getSignalProbes().size() == 1);
    const String probeId = document.graph().getSignalProbes().front().id;
    REQUIRE(document.revision() != beforeProbe);
    const auto beforePin = document.revision();
    REQUIRE(commands.pinComparisonSpy(probeId));
    REQUIRE(document.revision() == beforePin);
    REQUIRE(document.graph().getSignalProbes().size() == 1);
    REQUIRE_FALSE(commands.pinComparisonSpy(probeId));
    REQUIRE(document.graph().getSignalProbes().size() == 1);
    REQUIRE(document.presentation().comparisonSpyPins
            == std::vector<String> { probeId });
}

TEST_CASE("Editing loaded preset tags keeps the graph and later saves in sync",
        "[cycle-v2][preset][tags]") {
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto revision = document.revision();
    REQUIRE(commands.setPresetTags({ "Brass", "Sustained" }));
    REQUIRE(document.presentation().tags == StringArray { "Brass", "Sustained" });
    REQUIRE(document.revision() == revision);
    REQUIRE(document.isDirty());
    REQUIRE_FALSE(document.canUndo());
    const auto saved = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(saved.succeeded());
    REQUIRE(saved.presentation.tags == document.presentation().tags);
    REQUIRE(commands.setPresetTags({}));
    const auto cleared = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(cleared.succeeded());
    REQUIRE(cleared.presentation.tagsSpecified);
    REQUIRE(cleared.presentation.tags.isEmpty());
    REQUIRE_FALSE(commands.setPresetTags({}));
}

TEST_CASE("Renaming an open preset retains its title on the next graph save",
        "[cycle-v2][preset][title]") {
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto revision = document.revision();
    REQUIRE(commands.setPresetTitle("Copper Horn"));
    REQUIRE(document.presentation().title == "Copper Horn");
    REQUIRE(document.revision() == revision);
    REQUIRE(document.isDirty());
    REQUIRE_FALSE(document.canUndo());
    const auto saved = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(saved.succeeded());
    REQUIRE(saved.presentation.title == "Copper Horn");
}

TEST_CASE("Invalid preset MIDI is ignored without rejecting the graph",
        "[cycle-v2][preset][sequence][serialization]") {
    var encoded = GraphSerializer().writeJSON(graphWithOutput());
    auto presentation = std::make_unique<DynamicObject>();
    presentation->setProperty("version", 1);
    auto sequence = std::make_unique<DynamicObject>();
    sequence->setProperty("durationSeconds", 4.0);
    auto note = std::make_unique<DynamicObject>();
    note->setProperty("pitch", 200);
    note->setProperty("velocity", 100);
    note->setProperty("startSeconds", 0.0);
    note->setProperty("durationSeconds", 1.0);
    Array<var> notes;
    notes.add(var(note.release()));
    sequence->setProperty("notes", notes);
    presentation->setProperty("sequence", var(sequence.release()));
    encoded.getDynamicObject()->setProperty(
            "presetPresentation", var(presentation.release()));

    const GraphLoadResult loaded = GraphSerializer().loadJsonString(JSON::toString(encoded));
    REQUIRE(loaded.succeeded());
    REQUIRE_FALSE(loaded.presentation.sequence.has_value());
    REQUIRE(loaded.presentationWarning.isNotEmpty());
}

TEST_CASE("Time surface styles belong to each preset without changing graph or audio revisions",
        "[cycle-v2][preset][surface-program]") {
    const String savedStyle = GENERATE("icy-hot", "icy-hot-raw");
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto revision = document.revision();
    const auto graphJson = GraphSerializer().toJsonString(document.graph());
    const auto oldPreset = document.toJson();
    REQUIRE_FALSE(document.isDirty());
    REQUIRE(commands.setTimeSurfaceStyle("bullion"));
    REQUIRE(document.isDirty());
    REQUIRE(document.revision() == revision);
    REQUIRE(GraphSerializer().toJsonString(document.graph()) == graphJson);
    REQUIRE_FALSE(document.canUndo());
    const auto bullion = document.toJson();
    REQUIRE(commands.setTimeSurfaceStyle("recipe-19"));
    REQUIRE(document.loadJson(bullion, false));
    REQUIRE(document.presentation().timeSurfaceStyle == "bullion");
    REQUIRE(document.loadJson(oldPreset, false));
    REQUIRE(TimeSurfaceStyles::fromId(document.presentation().timeSurfaceStyle)
            == ScalarSurfaceTimeStyle::IcyHotRaw);

    const File temporary = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("surface-style-preset", ".cyclegraph");
    REQUIRE(commands.setTimeSurfaceStyle(savedStyle));
    REQUIRE(document.save(temporary));
    REQUIRE_FALSE(document.isDirty());
    REQUIRE(commands.setTimeSurfaceStyle("recipe-15"));
    REQUIRE(document.load(temporary));
    REQUIRE_FALSE(document.isDirty());
    REQUIRE(document.presentation().timeSurfaceStyle == savedStyle);
    REQUIRE(commands.addNode(NodeKind::WaveSource, { 30.f, 40.f }).succeeded());
    REQUIRE(commands.setTimeSurfaceStyle("bullion"));
    REQUIRE(document.undo());
    REQUIRE(document.isDirty());
    REQUIRE(document.presentation().timeSurfaceStyle == "bullion");
    REQUIRE(temporary.deleteFile());
}

TEST_CASE("Bipolar spectral surface styles persist independently of the audio graph",
        "[cycle-v2][preset][surface-program]") {
    GraphDocument document(graphWithOutput());
    GraphCommandDispatcher commands(document);
    const auto revision = document.revision();
    const auto graphJson = GraphSerializer().toJsonString(document.graph());
    const auto original = document.toJson();

    REQUIRE(commands.setBipolarSpectralSurfaceStyle("icy-hot"));
    REQUIRE(document.isDirty());
    REQUIRE(document.revision() == revision);
    REQUIRE(GraphSerializer().toJsonString(document.graph()) == graphJson);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.presentation().timeSurfaceStyle.isEmpty());

    const File temporary = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("spectral-surface-style-preset", ".cyclegraph");
    REQUIRE(document.save(temporary));
    REQUIRE(commands.setBipolarSpectralSurfaceStyle("greyscale"));
    REQUIRE(document.load(temporary));
    REQUIRE(document.presentation().bipolarSpectralSurfaceStyle == "icy-hot");
    REQUIRE(document.loadJson(original, false));
    REQUIRE(document.presentation().bipolarSpectralSurfaceStyle.isEmpty());
    REQUIRE(temporary.deleteFile());
}

TEST_CASE("Malformed optional preset preview does not invalidate its graph",
        "[cycle-v2][preset][presentation][serialization]") {
    var encoded = GraphSerializer().writeJSON(graphWithOutput());
    auto presentation = std::make_unique<DynamicObject>();
    presentation->setProperty("version", 1);
    auto preview = std::make_unique<DynamicObject>();
    preview->setProperty("mediaType", "image/jpeg");
    preview->setProperty("width", 320);
    preview->setProperty("height", 180);
    preview->setProperty("view", "spectrum");
    preview->setProperty("data", "not jpeg data");
    presentation->setProperty("preview", var(preview.release()));
    encoded.getDynamicObject()->setProperty(
            "presetPresentation",
            var(presentation.release()));

    const GraphLoadResult loaded = GraphSerializer().readJSON(encoded);

    REQUIRE(loaded.succeeded());
    REQUIRE_FALSE(loaded.presentation.preview.has_value());
    REQUIRE(loaded.presentationWarning.isNotEmpty());
}

TEST_CASE("Preset metadata can be read without decoding its preview",
        "[cycle-v2][preset][presentation][metadata]") {
    PresetPresentation presentation;
    presentation.author = "Daven";
    presentation.tags = { "acid" };
    presentation.preview = jpegPreview();

    const auto decoded = PresetPresentationCodec::readMetadataJSON(
            PresetPresentationCodec::writeJSON(presentation));

    REQUIRE(decoded.presentation.author == "Daven");
    REQUIRE(decoded.presentation.tags == StringArray { "acid" });
    REQUIRE_FALSE(decoded.presentation.preview.has_value());
    REQUIRE(decoded.warning.isEmpty());
}

TEST_CASE("Graph document preserves preset presentation through graph history",
        "[cycle-v2][preset][presentation][document]") {
    GraphDocument document(graphWithOutput());
    PresetPresentation presentation;
    presentation.preview = jpegPreview();
    document.setPresentation(presentation);

    NodeGraph before = document.graph();
    NodeGraph changed = before;
    changed.addNode(GraphNodeFactory().createNode(NodeKind::GlobalInput, "global", {}));
    document.recordExternalChange(std::move(before));

    REQUIRE(document.undo());
    REQUIRE(document.presentation().preview.has_value());
    REQUIRE(document.redo());
    REQUIRE(document.presentation().preview.has_value());

    const GraphLoadResult saved = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(saved.presentation.preview.has_value());
}
