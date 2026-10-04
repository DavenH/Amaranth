#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "UI/PatternBrowser.h"
#include "Graph/PatternLibrary.h"

using namespace CycleV2;
using namespace juce;

namespace {

Component* findChild(Component& parent, const String& id) {
    for (int index = 0; index < parent.getNumChildComponents(); ++index) {
        auto* child = parent.getChildComponent(index);
        if (child->getComponentID() == id) {
            return child;
        }
        if (auto* found = findChild(*child, id)) {
            return found;
        }
    }
    return nullptr;
}

}

TEST_CASE("Pattern IDs round trip without embedding MIDI events",
        "[cycle-v2][pattern][serialization]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "output", {}));
    PresetPresentation presentation;
    presentation.patternId = "factory-basic-sustained";
    PresetMidiSequence legacy;
    legacy.notes.push_back({ 60, 90, 0.0, 0.5 });
    presentation.sequence = legacy;
    const String json = GraphSerializer().toJsonString(graph, presentation);
    const auto loaded = GraphSerializer().loadJsonString(json);
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.presentation.patternId == presentation.patternId);
    REQUIRE_FALSE(loaded.presentation.sequence.has_value());
    REQUIRE(json.contains("patternId"));
    REQUIRE_FALSE(json.contains("\"sequence\""));
}

TEST_CASE("Pattern library validates and updates a stable user ID",
        "[cycle-v2][pattern][library]") {
    const File temporary = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-pattern-test-" + Uuid().toString());
    const File factory = temporary.getChildFile("factory");
    const File user = temporary.getChildFile("user");
    REQUIRE(factory.createDirectory().wasOk());
    PatternLibrary library(factory, user);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 6.0;
    phrase.notes.push_back({ 55, 72, 0.0, 1.0 });
    phrase.controls.push_back({ 1, 95, 2.0 });
    const String id = library.newUserId();
    const auto created = library.saveUserPattern(id, "Jazz Phrase", phrase, "Keys");
    REQUIRE(created.has_value());
    REQUIRE(created->name == "Jazz Phrase");
    REQUIRE(created->sequence.controls.size() == 1);
    REQUIRE(created->tag == "Keys");

    phrase.notes[0].velocity = 109;
    const auto updated = library.saveUserPattern(id, "Jazz Phrase", phrase);
    REQUIRE(updated.has_value());
    library.reload();
    REQUIRE(library.records().size() == 1);
    REQUIRE(library.find(id)->sequence.notes[0].velocity == 109);
    REQUIRE(library.find(id)->tag == "Keys");

    user.getChildFile("invalid.cyclepattern").replaceWithText("{ bad json");
    library.reload();
    REQUIRE(library.records().size() == 1);
    REQUIRE(temporary.deleteRecursively());
}

TEST_CASE("Factory preset references resolve through the curated pattern library",
        "[cycle-v2][pattern][factory]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File content = File(CYCLE_V2_SOURCE_DIR).getChildFile("content");
    PatternLibrary library(content.getChildFile("patterns"), {});
    REQUIRE(library.records().size() >= 5);
    REQUIRE(library.records().size() <= 8);
    REQUIRE(library.find("factory-sax-blue-hour") != nullptr);
    REQUIRE(library.find("factory-sax-blue-hour")->id == "factory-basic-sustained");
    REQUIRE(library.find("factory-acid-turn")->id == "factory-basic-bass");
    REQUIRE(library.find("factory-skyward-arc")->id == "factory-basic-lead");
    REQUIRE(library.find("factory-suspended-cloud")->id == "factory-basic-pad");
    REQUIRE(library.find("factory-swing-comp")->id == "factory-basic-keys");
    REQUIRE(library.find("factory-tom-steps")->id == "factory-basic-rhythm");
    for (const auto& record : library.records()) {
        REQUIRE(record.tag.isNotEmpty());
        REQUIRE(record.sequence.notes.size() >= 4);
    }
    int references = 0;
    for (const auto& file : content.getChildFile("presets").findChildFiles(
            File::findFiles, false, "*.cyclegraph")) {
        const var root = JSON::parse(file);
        const auto* object = root.getDynamicObject();
        REQUIRE(object != nullptr);
        const auto presentation = PresetPresentationCodec::readMetadataJSON(
                object->getProperty("presetPresentation")).presentation;
        if (presentation.patternId.startsWith("factory-")) {
            REQUIRE(library.find(presentation.patternId) != nullptr);
            REQUIRE_FALSE(presentation.sequence.has_value());
            ++references;
        }
    }
    REQUIRE(references > 0);
  #endif
}

TEST_CASE("Pattern browser rows show notes and assign their stable ID",
        "[cycle-v2][pattern][ui]") {
    ScopedJuceInitialiser_GUI gui;
    PresetMidiSequence phrase;
    phrase.durationSeconds = 4.0;
    phrase.notes.push_back({ 48, 80, 0.0, 1.0 });
    phrase.notes.push_back({ 55, 105, 1.5, 0.5 });
    String selected;
    String createdName;
    PatternBrowser browser(
            [&](const String& id) { selected = id; },
            [](const String&) {},
            [&](const String& name, const String& tag) {
                createdName = name + ":" + tag;
            });
    browser.setSize(310, 240);
    std::vector<PatternRecord> records;
    for (int index = 0; index < 8; ++index) {
        records.push_back({ "factory-" + String(index), "Pattern " + String(index),
                phrase, {}, true, index % 2 == 0 ? "Keys" : "Bass" });
    }
    browser.setRecords(records, {});
    const Image image = browser.createComponentSnapshot(browser.getLocalBounds());
    REQUIRE(image.isValid());
    REQUIRE(image.getWidth() == 310);
    auto* list = findChild(browser, "workspace.sidebar.patternList");
    REQUIRE(list != nullptr);
    auto* viewport = dynamic_cast<Viewport*>(
            findChild(browser, "workspace.sidebar.patternViewport"));
    REQUIRE(viewport != nullptr);
    const int rowHeight = list->getHeight() / (int) records.size();
    viewport->setViewPosition(0, 3 * rowHeight);
    const int scrollBeforeSelection = viewport->getViewPositionY();
    REQUIRE(scrollBeforeSelection > 0);
    const Time now = Time::getCurrentTime();
    const Point<float> rowPosition { 40.f, 3.f * rowHeight + 40.f };
    MouseEvent click(Desktop::getInstance().getMainMouseSource(),
            rowPosition, ModifierKeys::leftButtonModifier,
            1.f, 0.f, 0.f, 0.f, 0.f, list, list,
            now, rowPosition, now, 1, false);
    list->mouseUp(click);
    REQUIRE(selected == "factory-3");
    browser.setRecords(records, selected);
    REQUIRE(viewport->getViewPositionY() == scrollBeforeSelection);
    auto* filter = dynamic_cast<ComboBox*>(
            findChild(browser, "workspace.sidebar.patternType"));
    REQUIRE(filter != nullptr);
    filter->setSelectedId(3, sendNotificationSync);
    REQUIRE(list->getHeight() == 4 * rowHeight);
    auto* search = dynamic_cast<LibrarySearchField*>(
            findChild(browser, "workspace.sidebar.patternSearch"));
    auto* create = dynamic_cast<Button*>(
            findChild(browser, "workspace.sidebar.patternNew"));
    REQUIRE(search != nullptr);
    REQUIRE(create != nullptr);
    search->setText("Pattern 1", true);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(list->getHeight() == rowHeight);
    search->setText({}, true);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    create->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* prompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(prompt != nullptr);
    prompt->getTextEditor("name")->setText("Swing Variations");
    prompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(createdName == "Swing Variations:Bass");
}
