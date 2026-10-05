#include <catch2/catch_test_macros.hpp>

#include "Graph/PresetTagStore.h"
#include "UI/LibrarySearchField.h"
#include "UI/PresetBrowserPage.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Preset browser coalesces search and opens the highlighted card",
        "[cycle-v2][preset][browser]") {
    ScopedJuceInitialiser_GUI juce;
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File directory = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content")
            .getChildFile("presets");
    REQUIRE(directory.getChildFile("kicker.cyclegraph").existsAsFile());
    File opened;
    int browseCount {};
    int closeCount {};
    int playbackToggles {};
    PresetBrowserPage page(
            { directory },
            [&](const File& file) {
                opened = file;
                return true;
            },
            [&] { ++browseCount; },
            [&] { ++closeCount; },
            [&] { ++playbackToggles; },
            [](const File&, const StringArray&) {});
    page.setBounds(0, 0, 1000, 700);
    auto* search = dynamic_cast<TextEditor*>(page.findChildWithID("presetBrowser.search"));
    auto* viewport = dynamic_cast<Viewport*>(page.findChildWithID("presetBrowser.viewport"));
    auto* grid = viewport == nullptr
            ? nullptr
            : dynamic_cast<PresetCardGrid*>(viewport->getViewedComponent());
    auto* detail = page.findChildWithID("presetBrowser.detail");
    auto* sidebar = page.findChildWithID("presetBrowser.sidebar");
    auto* open = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.open"));
    auto* editTags = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.editTags"));
    auto* browse = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.browse"));
    auto* close = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.close"));
    REQUIRE(search != nullptr);
    REQUIRE(dynamic_cast<LibrarySearchField*>(search) != nullptr);
    REQUIRE(search->keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search->getText().isEmpty());
    REQUIRE(playbackToggles == 1);
    REQUIRE(page.keyPressed(KeyPress(KeyPress::spaceKey)));
    REQUIRE(playbackToggles == 2);
    REQUIRE(grid != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(detail != nullptr);
    REQUIRE(sidebar != nullptr);
    REQUIRE(open != nullptr);
    REQUIRE(editTags != nullptr);
    REQUIRE(browse != nullptr);
    REQUIRE(close != nullptr);
    for (int attempt = 0; attempt < 100 && !editTags->isEnabled(); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(grid->visibleCount() > 1);
    REQUIRE(editTags->isEnabled());
    REQUIRE(viewport->getWidth() > sidebar->getWidth());
    REQUIRE(detail->getWidth() > 200);
    REQUIRE(detail->getBounds().contains(open->getBounds()));
    REQUIRE(detail->getBounds().contains(editTags->getBounds()));
    REQUIRE(browse->getY() == close->getY());
    REQUIRE(browse->getBottom() == close->getBottom());

    search->setText("kicker", true);
    search->setText("acid-stab", true);
    for (int attempt = 0; attempt < 10 && grid->visibleCount() < 2; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(grid->visibleCount() >= 2);
    REQUIRE(grid->selectedRecord() != nullptr);
    const File first = grid->selectedRecord()->file;
    REQUIRE(page.keyPressed(KeyPress(KeyPress::rightKey)));
    REQUIRE(grid->selectedVisibleIndex() == 1);
    REQUIRE(grid->selectedRecord()->file != first);
    const File highlighted = grid->selectedRecord()->file;

    REQUIRE(page.keyPressed(KeyPress(KeyPress::returnKey)));
    REQUIRE(opened == highlighted);
    REQUIRE(closeCount == 1);
    REQUIRE(browseCount == 0);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Preset tag edits preserve other JSON fields",
        "[cycle-v2][preset][tags]") {
    ScopedJuceInitialiser_GUI juce;
    TemporaryFile temporary(File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-tag-edit.cyclegraph"));
    const File file = temporary.getFile();
    REQUIRE(file.replaceWithText(R"({
        "graph": { "id": "keep" },
        "presetPresentation": {
            "version": 1,
            "author": "Daven",
            "tags": ["Lead"],
            "unknown": "keep"
        },
        "other": "keep"
    })"));

    String error;
    REQUIRE(PresetTagStore::save(file,
            { " Brass ", "Sustained", "brass", "" }, error));
    juce::var root;
    REQUIRE(JSON::parse(file.loadFileAsString(), root).wasOk());
    const auto* object = root.getDynamicObject();
    REQUIRE(object != nullptr);
    REQUIRE(object->getProperty("other").toString() == "keep");
    REQUIRE(object->getProperty("graph").getDynamicObject()
            ->getProperty("id").toString() == "keep");
    const auto metadata = object->getProperty("presetPresentation");
    REQUIRE(metadata.getDynamicObject()->getProperty("unknown").toString() == "keep");
    REQUIRE(PresetPresentationCodec::readMetadataJSON(metadata).presentation.tags
            == StringArray { "Brass", "Sustained" });

    REQUIRE_FALSE(PresetTagStore::save(file, { "  " }, error));
    REQUIRE(error.isNotEmpty());
}

TEST_CASE("Curated preset families and distorted trait match saved metadata",
        "[cycle-v2][preset][tags]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File directory = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content").getChildFile("presets");
    Array<File> files;
    directory.findChildFiles(files, File::findFiles, false, "*.cyclegraph");
    int saxCount {};
    StringArray distortedNames;
    for (const auto& file : files) {
        const auto name = file.getFileNameWithoutExtension();
        juce::var root;
        REQUIRE(JSON::parse(file.loadFileAsString(), root).wasOk());
        const auto metadata = root.getDynamicObject()
                ->getProperty("presetPresentation");
        const auto tags = PresetPresentationCodec::readMetadataJSON(metadata)
                .presentation.tags;
        StringArray rawTags;
        const auto encodedTagsValue = metadata.getDynamicObject()
                ->getProperty("tags");
        const auto* encodedTags = encodedTagsValue.getArray();
        REQUIRE(encodedTags != nullptr);
        for (const auto& encoded : *encodedTags) {
            rawTags.add(encoded.toString());
        }
        auto uniqueTags = rawTags;
        uniqueTags.removeDuplicates(true);
        REQUIRE(rawTags == uniqueTags);
        REQUIRE_FALSE(tags.contains("Sustained"));
        if (tags.contains("Distorted")) {
            distortedNames.add(name);
        }
        if (name == "i") {
            REQUIRE(tags == StringArray { "Vocal" });
        }
        if (name == "satisfaction") {
            REQUIRE(tags == StringArray { "Vocal" });
        }
        if (!name.containsIgnoreCase("sax")
                && name != "kicker" && name != "stomper") {
            continue;
        }
        REQUIRE(tags[0] == (name.containsIgnoreCase("sax") ? "Brass" : "Bass"));
        if (name.containsIgnoreCase("sax")) {
            ++saxCount;
        }
    }
    REQUIRE(saxCount == 7);
    distortedNames.sortNatural();
    REQUIRE(distortedNames == StringArray {
            "fuzz-bass", "fuzz-square", "thrash-guitar", "thrash-guitar-3" });
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("A tag edit refreshes indexed search metadata",
        "[cycle-v2][preset][tags]") {
    ScopedJuceInitialiser_GUI juce;
    const File directory = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-tag-index-" + Uuid().toString());
    REQUIRE(directory.createDirectory().wasOk());
    const File file = directory.getChildFile("tag-fixture.cyclegraph");
    REQUIRE(file.replaceWithText(
            R"({"presetPresentation":{"version":1,"tags":["Lead"]}})"));

    std::vector<PresetLibraryRecord> records;
    std::vector<int> visible;
    {
        PresetLibraryIndex index({ directory }, [&](const auto& next,
                const auto& matching) {
            records = next;
            visible = matching;
        });
        index.start();
        for (int attempt = 0; attempt < 20
                && (records.empty() || !records.front().metadataReady); ++attempt) {
            MessageManager::getInstance()->runDispatchLoopUntil(100);
        }
        REQUIRE(records.size() == 1);
        REQUIRE(records.front().presentation.tags == StringArray { "Lead" });

        String error;
        REQUIRE(PresetTagStore::save(file, { "Brass" }, error));
        index.refreshRecord(file);
        REQUIRE(records.front().presentation.tags == StringArray { "Brass" });
        REQUIRE(records.front().searchText.contains("brass"));
        index.setQuery("brass");
        for (int attempt = 0; attempt < 10 && visible.empty(); ++attempt) {
            MessageManager::getInstance()->runDispatchLoopUntil(100);
        }
        REQUIRE(visible == std::vector<int> { 0 });
    }
    REQUIRE(directory.deleteRecursively());
}
