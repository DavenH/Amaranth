#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "UI/PatternBrowser.h"
#include "UI/SidebarMediaRow.h"
#include "Graph/PatternLibrary.h"
#include "UI/SidebarTagCloud.h"

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

bool clickTag(SidebarTagCloud& cloud, const String& tag) {
    for (const auto& [id, bounds] : cloud.pointerTargetsForAutomation()) {
        if (id != "workspace.sidebar.tag." + tag.toLowerCase()) {
            continue;
        }
        const auto position = bounds.getCentre();
        const Time now = Time::getCurrentTime();
        const MouseEvent click(Desktop::getInstance().getMainMouseSource(),
                position, ModifierKeys::leftButtonModifier,
                1.f, 0.f, 0.f, 0.f, 0.f, &cloud, &cloud,
                now, position, now, 1, false);
        cloud.mouseUp(click);
        return true;
    }
    return false;
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
    const auto created = library.saveUserPattern(id, "Jazz Phrase", phrase,
            "Keys", { "Keys", "Chords" });
    REQUIRE(created.has_value());
    REQUIRE(created->name == "Jazz Phrase");
    REQUIRE(created->sequence.controls.size() == 1);
    REQUIRE(created->tag == "Keys");
    REQUIRE(created->tags.contains("Keys"));
    REQUIRE(created->tags.contains("Chords"));

    phrase.notes[0].velocity = 109;
    const auto updated = library.saveUserPattern(id, "Jazz Phrase", phrase);
    REQUIRE(updated.has_value());
    library.reload();
    REQUIRE(library.records().size() == 1);
    REQUIRE(library.find(id)->sequence.notes[0].velocity == 109);
    REQUIRE(library.find(id)->tag == "Keys");
    REQUIRE(library.find(id)->tags.contains("Keys"));
    REQUIRE(library.find(id)->tags.contains("Chords"));

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
    REQUIRE(library.records().size() >= 6);
    REQUIRE(library.find("factory-sax-blue-hour") != nullptr);
    REQUIRE(library.find("factory-sax-blue-hour")->id == "factory-basic-sustained");
    REQUIRE(library.find("factory-acid-turn")->id == "factory-basic-bass");
    REQUIRE(library.find("factory-skyward-arc")->id == "factory-basic-lead");
    REQUIRE(library.find("factory-suspended-cloud")->id == "factory-basic-pad");
    REQUIRE(library.find("factory-swing-comp")->id == "factory-basic-keys");
    REQUIRE(library.find("factory-tom-steps")->id == "factory-basic-rhythm");
    int curatedCount = 0;
    StringArray referencedIds;
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
        }
        if (library.find(presentation.patternId) != nullptr) {
            referencedIds.addIfNotAlreadyThere(presentation.patternId);
            ++references;
        }
        REQUIRE_FALSE(presentation.tags.isEmpty());
    }
    REQUIRE(references > 0);
    for (const auto& record : library.records()) {
        REQUIRE(record.tag.isNotEmpty());
        REQUIRE_FALSE(record.tags.isEmpty());
        if (record.id.startsWith("factory-basic-")) {
            REQUIRE(record.sequence.notes.size() >= 4);
            ++curatedCount;
        } else {
            REQUIRE(referencedIds.contains(record.id));
        }
    }
    REQUIRE(curatedCount == 6);
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
            [&](const String& name, const StringArray& tags) {
                createdName = name + ":" + tags.joinIntoString(",");
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
    auto* filter = dynamic_cast<SidebarTagCloud*>(
            findChild(browser, "workspace.sidebar.patternTags"));
    REQUIRE(filter != nullptr);
    REQUIRE(clickTag(*filter, "Bass"));
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
    prompt->getTextEditor("tags")->setText("Bass, Acid");
    prompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(createdName == "Swing Variations:Bass,Acid");
}

TEST_CASE("Pattern favorite star toggles without assigning and combines with the tag filter",
        "[cycle-v2][pattern][ui][favorites]") {
    ScopedJuceInitialiser_GUI gui;
    PropertiesFile::Options options;
    options.applicationName = "CycleV2PatternFavoriteInteraction";
    options.doNotSave = true;
    options.osxLibrarySubFolder = "Application Support";
    PropertiesFile properties(options);
    LibraryFavorites favorites(properties, File("/tmp/factory-presets"));
    String selected;
    PatternBrowser browser(
            [&](const String& id) { selected = id; },
            [](const String&) {},
            [](const String&, const StringArray&) {},
            &favorites);
    browser.setBounds(0, 0, 310, 300);
    PresetMidiSequence phrase;
    phrase.durationSeconds = 2.0;
    phrase.notes.push_back({ 48, 90, 0.0, 0.5 });
    std::vector<PatternRecord> records {
            { "bass-one", "Bass One", phrase, {}, true, "Bass" },
            { "lead-one", "Lead One", phrase, {}, true, "Lead" }
    };
    browser.setRecords(records, {});
    auto* list = findChild(browser, "workspace.sidebar.patternList");
    auto* filter = dynamic_cast<Button*>(
            findChild(browser, "workspace.sidebar.patternFavoritesOnly"));
    auto* tagCloud = dynamic_cast<SidebarTagCloud*>(
            findChild(browser, "workspace.sidebar.patternTags"));
    REQUIRE(list != nullptr);
    REQUIRE(filter != nullptr);
    REQUIRE(tagCloud != nullptr);

    const Point<float> starPosition { 28.f, 14.f };
    const Time now = Time::getCurrentTime();
    const MouseEvent starClick(Desktop::getInstance().getMainMouseSource(),
            starPosition, ModifierKeys::leftButtonModifier,
            1.f, 0.f, 0.f, 0.f, 0.f, list, list,
            now, starPosition, now, 1, false);
    list->mouseUp(starClick);
    REQUIRE(favorites.isPatternFavorite("bass-one"));
    REQUIRE(selected.isEmpty());
    filter->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(list->getHeight() == SidebarMediaRow::height);
    REQUIRE(clickTag(*tagCloud, "Lead"));
    REQUIRE(list->getHeight() == 0);
    REQUIRE(clickTag(*tagCloud, "Lead"));
    REQUIRE(list->getHeight() == SidebarMediaRow::height);
}
