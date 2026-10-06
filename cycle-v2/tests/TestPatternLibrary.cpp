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

bool clickTarget(SidebarTagCloud& cloud, const String& target) {
    for (const auto& [id, bounds] : cloud.pointerTargetsForAutomation()) {
        if (id != target) {
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

    const auto renamed = library.renameUserPattern(id, "Blue Keys");
    REQUIRE(renamed.has_value());
    REQUIRE(renamed->id == id);
    REQUIRE(renamed->name == "Blue Keys");
    REQUIRE(renamed->sequence.notes[0].velocity == 109);
    REQUIRE(renamed->tags.contains("Chords"));
    library.reload();
    REQUIRE(library.find(id)->name == "Blue Keys");

    user.getChildFile("invalid.cyclepattern").replaceWithText("{ bad json");
    library.reload();
    REQUIRE(library.records().size() == 1);
    REQUIRE_FALSE(library.renameUserPattern("missing", "Unknown").has_value());
    REQUIRE_FALSE(library.deleteUserPattern("missing"));
    REQUIRE(library.deleteUserPattern(id));
    REQUIRE(library.find(id) == nullptr);
    REQUIRE_FALSE(user.getChildFile(id + ".cyclepattern").existsAsFile());
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
    REQUIRE_FALSE(library.renameUserPattern(
            "factory-basic-rhythm", "Renamed").has_value());
    REQUIRE_FALSE(library.deleteUserPattern("factory-basic-rhythm"));
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
    REQUIRE(clickTarget(*filter, "workspace.sidebar.tag.bass"));
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
    auto* tagCloud = dynamic_cast<SidebarTagCloud*>(
            findChild(browser, "workspace.sidebar.patternTags"));
    REQUIRE(list != nullptr);
    REQUIRE(tagCloud != nullptr);

    const Point<float> starPosition = SidebarMediaRow::patternFavoriteBounds(
            { 0.f, 0.f, (float) list->getWidth(),
                    (float) SidebarMediaRow::height }).getCentre();
    const Time now = Time::getCurrentTime();
    const MouseEvent starClick(Desktop::getInstance().getMainMouseSource(),
            starPosition, ModifierKeys::leftButtonModifier,
            1.f, 0.f, 0.f, 0.f, 0.f, list, list,
            now, starPosition, now, 1, false);
    list->mouseUp(starClick);
    REQUIRE(favorites.isPatternFavorite("bass-one"));
    REQUIRE(selected.isEmpty());
    REQUIRE(clickTarget(*tagCloud, "workspace.sidebar.favoriteFilter"));
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(tagCloud->favoritesOnly());
    REQUIRE(list->getHeight() == SidebarMediaRow::height);
    REQUIRE(clickTarget(*tagCloud, "workspace.sidebar.tag.lead"));
    REQUIRE(list->getHeight() == 0);
    REQUIRE(clickTarget(*tagCloud, "workspace.sidebar.tag.lead"));
    REQUIRE(list->getHeight() == SidebarMediaRow::height);
}

TEST_CASE("Pattern card uses the available row width and keeps notes below its header",
        "[cycle-v2][pattern][ui][layout]") {
    const juce::Rectangle<float> row { 0.f, 0.f, 250.f,
            (float) SidebarMediaRow::height };
    const auto card = SidebarMediaRow::patternCardBounds(row);
    const auto notes = SidebarMediaRow::patternPreviewBounds(row);
    REQUIRE(card.getX() == 2.f);
    REQUIRE(row.getRight() - card.getRight() == 2.f);
    REQUIRE(notes.getY() >= card.getY() + 31.f);
    REQUIRE(notes.getBottom() <= card.getBottom());
}

TEST_CASE("Pattern browser renames and deletes only selected user patterns",
        "[cycle-v2][pattern][ui][management]") {
    ScopedJuceInitialiser_GUI gui;
    String renamedId;
    String renamedTitle;
    String deletedId;
    String editedId;
    String createdTitle;
    int created {};
    PatternBrowser browser(
            [](const String&) {},
            [&](const String& id) { editedId = id; },
            [&](const String& name, const StringArray&) {
                ++created;
                createdTitle = name;
            },
            nullptr,
            [&](const String& id, const String& name) {
                renamedId = id;
                renamedTitle = name;
            },
            [&](const String& id) { deletedId = id; });
    browser.setBounds(0, 0, 310, 300);
    PresetMidiSequence phrase;
    phrase.notes.push_back({ 60, 90, 0.0, 1.0 });
    std::vector<PatternRecord> records {
            { "factory-pad", "Factory Pad", phrase, {}, true, "Pad" },
            { "user-pad", "My Pad", phrase, {}, false, "Pad" }
    };
    auto* edit = dynamic_cast<Button*>(
            findChild(browser, "workspace.sidebar.patternEdit"));
    auto* rename = dynamic_cast<Button*>(
            findChild(browser, "workspace.sidebar.patternRename"));
    auto* remove = dynamic_cast<Button*>(
            findChild(browser, "workspace.sidebar.patternDelete"));
    REQUIRE(edit != nullptr);
    REQUIRE(rename != nullptr);
    REQUIRE(remove != nullptr);

    browser.setRecords(records, "factory-pad");
    REQUIRE(edit->isEnabled());
    REQUIRE_FALSE(rename->isEnabled());
    REQUIRE_FALSE(remove->isEnabled());
    edit->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* copyPrompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(copyPrompt != nullptr);
    REQUIRE(copyPrompt->getTextEditorContents("name") == "Factory Pad Variation");
    copyPrompt->exitModalState(0);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(created == 0);

    edit->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    copyPrompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(copyPrompt != nullptr);
    copyPrompt->getTextEditor("name")->setText("Custom Pad");
    copyPrompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(created == 1);
    REQUIRE(createdTitle == "Custom Pad");

    browser.setRecords(records, "user-pad");
    REQUIRE(rename->isEnabled());
    REQUIRE(remove->isEnabled());
    edit->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(editedId == "user-pad");
    REQUIRE(created == 1);
    rename->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* renamePrompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(renamePrompt != nullptr);
    renamePrompt->getTextEditor("name")->setText("Evening Pad");
    renamePrompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(renamedId == "user-pad");
    REQUIRE(renamedTitle == "Evening Pad");

    remove->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* deletePrompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(deletePrompt != nullptr);
    deletePrompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(deletedId == "user-pad");
    browser.setRecords({ records.front() }, "user-pad");
    REQUIRE_FALSE(edit->isEnabled());
    REQUIRE_FALSE(rename->isEnabled());
    REQUIRE_FALSE(remove->isEnabled());
}
