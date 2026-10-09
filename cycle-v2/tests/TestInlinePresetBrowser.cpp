#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>

#include "UI/InlinePresetBrowser.h"
#include "Graph/PresetPresentation.h"
#include "UI/LibrarySearchField.h"
#include "UI/SidebarMediaRow.h"
#include "UI/SidebarTagCloud.h"

using namespace CycleV2;
using namespace juce;

namespace {

Component* findDescendantWithID(Component& parent, const String& id) {
    for (int index = 0; index < parent.getNumChildComponents(); ++index) {
        Component* child = parent.getChildComponent(index);
        if (child->getComponentID() == id) {
            return child;
        }
        if (Component* match = findDescendantWithID(*child, id)) {
            return match;
        }
    }
    return nullptr;
}

bool clickTag(SidebarTagCloud& cloud, const String& tag, bool rightClick = true) {
    for (const auto& [id, bounds] : cloud.pointerTargetsForAutomation()) {
        if (id != "workspace.sidebar.tag." + tag.toLowerCase()) {
            continue;
        }
        const auto position = bounds.getCentre();
        const Time now = Time::getCurrentTime();
        const MouseEvent click(Desktop::getInstance().getMainMouseSource(),
                position, rightClick ? ModifierKeys::rightButtonModifier : ModifierKeys::leftButtonModifier,
                1.f, 0.f, 0.f, 0.f, 0.f, &cloud, &cloud,
                now, position, now, 1, false);
        cloud.mouseUp(click);
        return true;
    }
    return false;
}

bool clickFavoriteFilter(SidebarTagCloud& cloud) {
    for (const auto& [id, bounds] : cloud.pointerTargetsForAutomation()) {
        if (id != "workspace.sidebar.favoriteFilter") {
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

Rectangle<float> targetBounds(
        const InlinePresetBrowser& browser,
        const String& id) {
    for (const auto& [targetId, bounds] : browser.pointerTargetsForAutomation()) {
        if (targetId == id) {
            return bounds;
        }
    }
    return {};
}

}

TEST_CASE("Preset search fields reserve Space for playback when empty",
        "[cycle-v2][preset][browser][search][regression]") {
    ScopedJuceInitialiser_GUI juce;
    LibrarySearchField search("Search presets...");
    int playbackToggles {};
    search.setPlaybackToggleCallback([&] { ++playbackToggles; });

    REQUIRE(search.keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search.getText().isEmpty());
    REQUIRE(playbackToggles == 1);

    search.setText("bass", false);
    search.setCaretPosition(search.getText().length());
    REQUIRE(search.keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search.getText() == "bass ");
    REQUIRE(playbackToggles == 1);

    search.clear();
    REQUIRE(search.keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search.getText().isEmpty());
    REQUIRE(playbackToggles == 2);

    search.setText("   ", false);
    REQUIRE(search.keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search.getText().isEmpty());
    REQUIRE(playbackToggles == 3);

    search.insertTextAtCaret(" ");
    REQUIRE(search.getText().isEmpty());
    REQUIRE(playbackToggles == 4);

    search.setText("  ", false);
    search.insertTextAtCaret(" ");
    REQUIRE(search.getText().isEmpty());
    REQUIRE(playbackToggles == 5);

    search.setText("bass", false);
    search.setCaretPosition(search.getText().length());
    search.insertTextAtCaret(" ");
    REQUIRE(search.getText() == "bass ");
    REQUIRE(playbackToggles == 5);
}

TEST_CASE("Sidebar tag chips narrow results across selected tags",
        "[cycle-v2][preset][browser][tags]") {
    ScopedJuceInitialiser_GUI juce;
    SidebarTagCloud cloud;
    cloud.setBounds(0, 0, 240, 60);
    cloud.setTags({ "Bass", "Keys", "Pad" });
    int changes {};
    cloud.setChangeCallback([&] { ++changes; });

    REQUIRE(cloud.matches({ "Bass" }));
    REQUIRE(clickTag(cloud, "Bass"));
    REQUIRE(cloud.matches({ "Bass", "Acid" }));
    REQUIRE_FALSE(cloud.matches({ "Pad" }));
    REQUIRE(clickTag(cloud, "Pad"));
    REQUIRE(cloud.matches({ "Bass", "Pad" }));
    REQUIRE_FALSE(cloud.matches({ "Pad" }));
    REQUIRE_FALSE(cloud.matches({ "Bass" }));
    REQUIRE_FALSE(cloud.matches({ "Keys" }));
    REQUIRE(changes == 2);

    cloud.setTags({ "Bass", "Pad", "Texture" });
    REQUIRE(cloud.selectedTags().size() == 2);
    REQUIRE(clickTag(cloud, "Bass"));
    REQUIRE(clickTag(cloud, "Pad"));
    REQUIRE(cloud.matches({ "Keys" }));
}

TEST_CASE("Preset and pattern actions occupy the same sidebar positions",
        "[cycle-v2][preset][browser][inline][layout]") {
    ScopedJuceInitialiser_GUI gui;
    InlinePresetBrowser browser({},
            [](const File&) { return true; },
            [] {}, [](WorkspaceSidebarTab) {});
    browser.configurePatterns({}, {}, {}, {}, {});
    browser.setBounds(0, 0, 272, 600);
    const std::array<String, 5> presetIds {
            "workspace.sidebar.search",
            "workspace.sidebar.presetNew",
            "workspace.sidebar.presetEdit",
            "workspace.sidebar.presetRename",
            "workspace.sidebar.delete"
    };
    const std::array<String, 5> patternIds {
            "workspace.sidebar.patternSearch",
            "workspace.sidebar.patternNew",
            "workspace.sidebar.patternEdit",
            "workspace.sidebar.patternRename",
            "workspace.sidebar.patternDelete"
    };
    std::array<Rectangle<float>, 5> presetBounds;
    for (size_t index = 0; index < presetBounds.size(); ++index) {
        presetBounds[index] = targetBounds(browser, presetIds[index]);
        REQUIRE_FALSE(presetBounds[index].isEmpty());
    }
    browser.setActiveTab(WorkspaceSidebarTab::Patterns);
    for (size_t index = 0; index < patternIds.size(); ++index) {
        REQUIRE(targetBounds(browser, patternIds[index]) == presetBounds[index]);
    }
}

TEST_CASE("Preset metadata stays left of the spectrogram at sidebar widths",
        "[cycle-v2][preset][browser][inline][layout]") {
    ScopedJuceInitialiser_GUI gui;
    for (const float width : { 250.f, 290.f }) {
        const Rectangle<float> row { 0.f, 0.f, width, (float) SidebarMediaRow::presetHeight };
        const auto card = SidebarMediaRow::cardBounds(row);
        const auto layout = SidebarMediaRow::presetLayout(row, { "Bass", "Phase Velocity" });
        REQUIRE(layout.preview.getWidth() == Catch::Approx(card.reduced(5.f).getWidth() * 0.6f));
        REQUIRE(card.contains(layout.preview));
        REQUIRE(card.contains(layout.favorite));
        REQUIRE_FALSE(layout.labels.title.intersects(layout.favorite));
        REQUIRE(layout.labels.title.getRight() < layout.preview.getX());
        REQUIRE(layout.labels.tagCount == 2);
        REQUIRE(layout.labels.title.getRight() == Catch::Approx(layout.preview.getX() - 6.f));
        REQUIRE(layout.favorite.getY() > layout.labels.title.getBottom());
        for (const auto& tag : layout.labels.tags) {
            REQUIRE(card.contains(tag));
            REQUIRE(tag.getY() > layout.labels.title.getBottom());
            REQUIRE(tag.getHeight() == layout.favorite.getHeight());
            REQUIRE(tag.getY() == layout.favorite.getY());
            REQUIRE(tag.getY() - layout.labels.title.getBottom() <= 4.f);
            REQUIRE_FALSE(tag.intersects(layout.favorite));
            REQUIRE(tag.getRight() < layout.preview.getX());
        }
        REQUIRE_FALSE(layout.labels.tags[0].intersects(layout.labels.tags[1]));
        REQUIRE(SidebarMediaRow::favoriteBounds(row) == layout.favorite);
    }
}

TEST_CASE("Inline preset sidebar switches views filters and loads with Return",
        "[cycle-v2][preset][browser][inline]") {
    ScopedJuceInitialiser_GUI juce;
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File directory = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content")
            .getChildFile("presets");
    File opened;
    int browseCount {};
    int createCount {};
    String deleteConfirmationName;
    File deleted;
    std::function<void(bool)> finishDeleteConfirmation;
    std::vector<WorkspaceSidebarTab> tabs;
    InlinePresetBrowser browser(
            { directory },
            [&](const File& file) {
                opened = file;
                return true;
            },
            [&] { ++browseCount; },
            [&](WorkspaceSidebarTab tab) { tabs.push_back(tab); },
            [&](const File& file) {
                deleted = file;
                return true;
            },
            [&](const String& name, std::function<void(bool)> completion) {
                deleteConfirmationName = name;
                finishDeleteConfirmation = std::move(completion);
            },
            [&] { ++createCount; });
    browser.setBounds(0, 0, 272, 760);

    auto* curves = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.curves"));
    auto* presets = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.presets"));
    auto* patterns = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.patterns"));
    auto* search = dynamic_cast<TextEditor*>(
            findDescendantWithID(browser, "workspace.sidebar.search"));
    auto* browse = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.browse"));
    auto* create = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.presetNew"));
    auto* edit = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.presetEdit"));
    auto* rename = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.presetRename"));
    auto* tagCloud = dynamic_cast<SidebarTagCloud*>(
            browser.findChildWithID("workspace.sidebar.presetTags"));
    auto* list = findDescendantWithID(browser, "workspace.sidebar.list");
    auto* viewport = dynamic_cast<Viewport*>(
            findDescendantWithID(browser, "workspace.sidebar.viewport"));
    auto* remove = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.delete"));
    REQUIRE(curves != nullptr);
    REQUIRE(presets != nullptr);
    REQUIRE(patterns != nullptr);
    REQUIRE(search != nullptr);
    REQUIRE(dynamic_cast<LibrarySearchField*>(search) != nullptr);
    int playbackToggles {};
    browser.setPlaybackToggleCallback([&] { ++playbackToggles; });
    REQUIRE(search->keyPressed(KeyPress(' ', ModifierKeys::noModifiers, ' ')));
    REQUIRE(search->getText().isEmpty());
    REQUIRE(playbackToggles == 1);
    REQUIRE(browse != nullptr);
    REQUIRE(create != nullptr);
    REQUIRE(edit != nullptr);
    REQUIRE(rename != nullptr);
    REQUIRE(tagCloud != nullptr);
    REQUIRE(list != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(remove != nullptr);
    REQUIRE(search->getFont().getHeight() >= 14.f);
    REQUIRE(search->getHeight() == 30);
    REQUIRE(create->getHeight() == 28);
    REQUIRE(search->getX() == create->getX());
    REQUIRE(create->getY() > search->getBottom());
    REQUIRE(create->getX() < edit->getX());
    REQUIRE(edit->getX() < rename->getX());
    REQUIRE(rename->getX() < remove->getX());
    REQUIRE(remove->getRight() == search->getRight());
    REQUIRE(create->getButtonText() == "+ NEW");
    REQUIRE(tagCloud->getY() > search->getY());
    REQUIRE_FALSE(findDescendantWithID(browser, "workspace.sidebar.hero"));
    create->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(createCount == 1);
    REQUIRE(browser.activeTab() == WorkspaceSidebarTab::Presets);
    REQUIRE(browser.hitTest(20, 300));

    curves->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browser.activeTab() == WorkspaceSidebarTab::Curves);
    REQUIRE_FALSE(browser.hitTest(20, 300));
    REQUIRE_FALSE(tagCloud->isVisible());
    presets->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browser.activeTab() == WorkspaceSidebarTab::Presets);
    REQUIRE(tagCloud->isVisible());
    const std::vector<WorkspaceSidebarTab> expectedTabs {
            WorkspaceSidebarTab::Curves,
            WorkspaceSidebarTab::Presets
    };
    REQUIRE(tabs == expectedTabs);

    for (int attempt = 0; attempt < 20 && browser.visiblePresetCount() < 2; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() > 1);
    REQUIRE(list->getHeight()
            == 20 + browser.visiblePresetCount() * SidebarMediaRow::presetHeight);
    const int unfilteredCount = browser.visiblePresetCount();
    for (int attempt = 0; attempt < 30 && !clickTag(*tagCloud, "Keys"); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(tagCloud->selectedTags().contains("Keys"));
    for (int attempt = 0; attempt < 30 && browser.visiblePresetCount() == 0; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() > 0);
    REQUIRE(browser.visiblePresetCount() < unfilteredCount);
    REQUIRE(clickTag(*tagCloud, "Keys"));
    REQUIRE(tagCloud->selectedTags().isEmpty());

    REQUIRE(clickTag(*tagCloud, "Distorted"));
    const int distortedCount = browser.visiblePresetCount();
    REQUIRE(distortedCount > 0);
    REQUIRE(distortedCount < unfilteredCount);
    REQUIRE(clickTag(*tagCloud, "Bass"));
    REQUIRE(browser.visiblePresetCount() > 0);
    REQUIRE(browser.visiblePresetCount() < distortedCount);
    REQUIRE(clickTag(*tagCloud, "Distorted"));
    REQUIRE(clickTag(*tagCloud, "Bass"));
    REQUIRE(tagCloud->selectedTags().isEmpty());

    viewport->setViewPosition(0, 100);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(viewport->getViewPositionY() == 100);
    REQUIRE(browser.keyPressed(KeyPress(KeyPress::downKey)));
    REQUIRE(viewport->getViewPositionY() == 100);

    search->setText("kicker", true);
    for (int attempt = 0; attempt < 12 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);
    const Time now = Time::getCurrentTime();
    const Point<float> rowPosition { 40.f, 50.f };
    MouseEvent click(Desktop::getInstance().getMainMouseSource(),
            rowPosition, ModifierKeys::leftButtonModifier,
            1.f, 0.f, 0.f, 0.f, 0.f, list, list,
            now, rowPosition, now, 1, false);
    list->mouseUp(click);
    REQUIRE(opened.getFileNameWithoutExtension() == "kicker");
    REQUIRE(browser.keyPressed(KeyPress(KeyPress::returnKey)));

    remove->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(deleteConfirmationName == "kicker");
    REQUIRE(finishDeleteConfirmation);
    REQUIRE(deleted == File());
    finishDeleteConfirmation(false);
    REQUIRE(deleted == File());

    remove->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(finishDeleteConfirmation);
    finishDeleteConfirmation(true);
    REQUIRE(deleted.getFileNameWithoutExtension() == "kicker");

    browse->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browseCount == 1);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Preset deletion confirmation uses compact ASCII-safe copy",
        "[cycle-v2][preset][browser][inline][regression]") {
    ScopedJuceInitialiser_GUI juce;
    Component applicationWindow;
    applicationWindow.setBounds(100, 80, 1200, 800);
    std::unique_ptr<AlertWindow> alert(
            applicationWindow.getLookAndFeel().createAlertWindow(
                    "Move preset to Trash?",
                    InlinePresetBrowser::deleteConfirmationMessage("acoustic"),
                    "Move to Trash",
                    "Cancel",
                    {},
                    MessageBoxIconType::WarningIcon,
                    2,
                    &applicationWindow));

    REQUIRE(InlinePresetBrowser::deleteConfirmationMessage("acoustic")
            == "\"acoustic\" will be removed from the preset library.");
    REQUIRE(alert != nullptr);
    REQUIRE(alert->getWidth() <= 600);
    REQUIRE(alert->getBounds().getCentre() == applicationWindow.getBounds().getCentre());
}

TEST_CASE("Preset row star and Favorites filter do not load the sound",
        "[cycle-v2][preset][browser][inline][favorites]") {
    ScopedJuceInitialiser_GUI gui;
  #if defined(CYCLE_V2_SOURCE_DIR)
    PropertiesFile::Options options;
    options.applicationName = "CycleV2InlineFavoriteInteraction";
    options.doNotSave = true;
    options.osxLibrarySubFolder = "Application Support";
    PropertiesFile properties(options);
    const File directory = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content").getChildFile("presets");
    LibraryFavorites favorites(properties, directory);
    File opened;
    InlinePresetBrowser browser(
            { directory },
            [&](const File& file) { opened = file; return true; },
            [] {}, [](WorkspaceSidebarTab) {}, {}, {}, {}, &favorites);
    browser.setBounds(0, 0, 272, 760);
    auto* search = dynamic_cast<TextEditor*>(
            findDescendantWithID(browser, "workspace.sidebar.search"));
    auto* filter = dynamic_cast<SidebarTagCloud*>(
            browser.findChildWithID("workspace.sidebar.presetTags"));
    auto* list = findDescendantWithID(browser, "workspace.sidebar.list");
    REQUIRE(search != nullptr);
    REQUIRE(filter != nullptr);
    REQUIRE(list != nullptr);
    search->setText("kicker", true);
    for (int attempt = 0; attempt < 30 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);

    const auto firstRow = Rectangle<float>(0.f, 0.f,
            (float) list->getWidth(), (float) SidebarMediaRow::presetHeight);
    const Point<float> position = SidebarMediaRow::favoriteBounds(firstRow)
            .getCentre();
    const Time now = Time::getCurrentTime();
    const MouseEvent click(Desktop::getInstance().getMainMouseSource(),
            position, ModifierKeys::leftButtonModifier,
            1.f, 0.f, 0.f, 0.f, 0.f, list, list,
            now, position, now, 1, false);
    list->mouseUp(click);
    REQUIRE(favorites.isPresetFavorite(directory.getChildFile("kicker.cyclegraph")));
    REQUIRE(opened == File());
    REQUIRE(clickFavoriteFilter(*filter));
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browser.visiblePresetCount() == 1);
    list->mouseUp(click);
    REQUIRE_FALSE(favorites.isPresetFavorite(directory.getChildFile("kicker.cyclegraph")));
    REQUIRE(browser.visiblePresetCount() == 0);
    REQUIRE(opened == File());
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Preset sidebar edits tags and title through its grouped actions",
        "[cycle-v2][preset][browser][inline][actions]") {
    ScopedJuceInitialiser_GUI gui;
    const File directory = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-sidebar-actions-" + Uuid().toString());
    REQUIRE(directory.createDirectory().wasOk());
    const File file = directory.getChildFile("seed.cyclegraph");
    REQUIRE(file.replaceWithText(R"({
        "presetPresentation": { "version": 1, "tags": ["Lead"] },
        "other": "keep"
    })"));

    int opened {};
    String savedTitle;
    StringArray savedTags;
    InlinePresetBrowser browser({ directory },
            [&](const File&) { ++opened; return true; },
            [] {}, [](WorkspaceSidebarTab) {});
    browser.setMetadataChangedCallbacks(
            [&](const File& changedFile, const String& title) {
                REQUIRE(changedFile == file);
                savedTitle = title;
            },
            [&](const File& changedFile, const StringArray& tags) {
                REQUIRE(changedFile == file);
                savedTags = tags;
            });
    browser.setBounds(0, 0, 272, 600);
    for (int attempt = 0; attempt < 30 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);
    auto* edit = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.presetEdit"));
    auto* rename = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.presetRename"));
    auto* search = dynamic_cast<TextEditor*>(
            findDescendantWithID(browser, "workspace.sidebar.search"));
    REQUIRE(edit != nullptr);
    REQUIRE(rename != nullptr);
    REQUIRE(search != nullptr);
    REQUIRE(edit->isEnabled());
    REQUIRE(rename->isEnabled());

    edit->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    auto* prompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(prompt != nullptr);
    prompt->getTextEditor("tags")->setText("Brass, Expressive");
    prompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(savedTags == StringArray { "Brass", "Expressive" });

    rename->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    prompt = dynamic_cast<AlertWindow*>(
            ModalComponentManager::getInstance()->getModalComponent(0));
    REQUIRE(prompt != nullptr);
    prompt->getTextEditor("title")->setText("Brass Hall");
    prompt->exitModalState(1);
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(savedTitle == "Brass Hall");
    REQUIRE(file.existsAsFile());
    REQUIRE(JSON::parse(file.loadFileAsString()).getDynamicObject()
            ->getProperty("other").toString() == "keep");
    search->setText("Brass Hall", true);
    for (int attempt = 0; attempt < 30 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);
    REQUIRE(opened == 0);
    REQUIRE(directory.deleteRecursively());
}

TEST_CASE("Nodes tab exposes palette hits while consuming empty sidebar space",
        "[cycle-v2][browser][inline][palette]") {
    ScopedJuceInitialiser_GUI gui;
    WorkspaceSidebarTab selected = WorkspaceSidebarTab::Presets;
    InlinePresetBrowser browser({}, [](const File&) { return true; }, [] {},
            [&selected](WorkspaceSidebarTab tab) { selected = tab; });
    browser.setBounds(0, 0, 290, 700);
    browser.nodePaletteHitTest = [](Point<int> point) {
        return Rectangle<int>(80, 80, 40, 40).contains(point);
    };
    REQUIRE_FALSE(targetBounds(browser, "workspace.sidebar.nodes").isEmpty());
    browser.setActiveTab(WorkspaceSidebarTab::Nodes);
    REQUIRE(selected == WorkspaceSidebarTab::Nodes);
    REQUIRE_FALSE(browser.hitTest(100, 100));
    REQUIRE(browser.hitTest(10, 300));
    REQUIRE(browser.hitTest(100, 20));
    REQUIRE(targetBounds(browser, "workspace.sidebar.search").isEmpty());
    browser.setActiveTab(WorkspaceSidebarTab::Presets);
    REQUIRE(browser.hitTest(100, 100));
    REQUIRE_FALSE(targetBounds(browser, "workspace.sidebar.search").isEmpty());
}

TEST_CASE("Preset tag cloud separates filtering from selected membership and left-click edits",
        "[cycle-v2][preset][browser][inline][tag-edit]") {
    ScopedJuceInitialiser_GUI gui;
    const auto directory = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-tag-edit-" + Uuid().toString());
    REQUIRE(directory.createDirectory().wasOk());
    const auto bass = directory.getChildFile("a-bass.cyclegraph");
    const auto pad = directory.getChildFile("b-pad.cyclegraph");
    REQUIRE(bass.replaceWithText(R"({"presetPresentation":{"version":1,"tags":["Bass"]},"other":"keep"})"));
    REQUIRE(pad.replaceWithText(R"({"presetPresentation":{"version":1,"tags":["Pad"]}})"));
    {
        int opens = 0;
        int saves = 0;
        InlinePresetBrowser browser({ directory }, [&](const File&) { ++opens; return true; },
                [] {}, [](WorkspaceSidebarTab) {});
        browser.setMetadataChangedCallbacks({}, [&](const File& file, const StringArray&) {
            REQUIRE(file == bass);
            ++saves;
        });
        browser.setBounds(0, 0, 290, 700);
        for (int attempt = 0; attempt < 40 && browser.visiblePresetCount() != 2; ++attempt) {
            MessageManager::getInstance()->runDispatchLoopUntil(50);
        }
        REQUIRE(browser.visiblePresetCount() == 2);
        auto* cloud = dynamic_cast<SidebarTagCloud*>(findDescendantWithID(browser, "workspace.sidebar.presetTags"));
        REQUIRE(cloud != nullptr);
        const auto blue = cloud->tagAccent("Bass");
        REQUIRE(blue == Colour(0xff6d9ed8));
        REQUIRE(cloud->tagAccent("Pad").isTransparent());
        REQUIRE(browser.keyPressed(KeyPress(KeyPress::downKey)));
        REQUIRE(cloud->tagAccent("Bass").isTransparent());
        REQUIRE(cloud->tagAccent("Pad") == blue);
        REQUIRE(browser.keyPressed(KeyPress(KeyPress::upKey)));
        REQUIRE(clickTag(*cloud, "Bass"));
        REQUIRE(browser.visiblePresetCount() == 1);
        REQUIRE(cloud->tagAccent("Bass") == Colour(0xffad83da));
        REQUIRE(clickTag(*cloud, "Bass", false));
        REQUIRE(browser.visiblePresetCount() == 0);
        REQUIRE(cloud->selectedTags() == StringArray { "Bass" });
        REQUIRE(cloud->tagAccent("Bass") == Colour(0xffd16fab));
        auto root = JSON::parse(bass.loadFileAsString());
        auto metadata = PresetPresentationCodec::readMetadataJSON(root["presetPresentation"]).presentation;
        REQUIRE(metadata.tagsSpecified);
        REQUIRE(metadata.tags.isEmpty());
        REQUIRE(root["other"].toString() == "keep");
        const auto roundTrip = PresetPresentationCodec::readMetadataJSON(
                PresetPresentationCodec::writeJSON(metadata)).presentation;
        REQUIRE(roundTrip.tagsSpecified);
        REQUIRE(roundTrip.tags.isEmpty());
        REQUIRE(clickTag(*cloud, "Bass"));
        REQUIRE(browser.visiblePresetCount() == 2);
        REQUIRE(clickTag(*cloud, "Pad", false));
        REQUIRE(cloud->tagAccent("Pad") == blue);
        REQUIRE(clickTag(*cloud, "Pad", false));
        REQUIRE(cloud->tagAccent("Pad").isTransparent());
        REQUIRE(cloud->selectedTags().isEmpty());
        REQUIRE(saves == 3);
        REQUIRE(opens == 0);
        browser.refreshRecord(bass);
        MessageManager::getInstance()->runDispatchLoopUntil(300);
        REQUIRE(cloud->tagAccent("Pad").isTransparent());
        REQUIRE(cloud->tagAccent("Bass").isTransparent());
    }
    REQUIRE(directory.deleteRecursively());
}

TEST_CASE("Tag cloud paints distinct membership filter and combined colours",
        "[cycle-v2][preset][browser][inline][tag-edit]") {
    ScopedJuceInitialiser_GUI gui;
    SidebarTagCloud cloud;
    cloud.setBounds(0, 0, 290, 54);
    cloud.setTags({ "Bass", "Lead", "Pad" });
    cloud.setRecordTags({ "Bass", "Lead" });
    REQUIRE(clickTag(cloud, "Lead"));
    REQUIRE(clickTag(cloud, "Pad"));
    const auto rendered = cloud.createComponentSnapshot(cloud.getLocalBounds());
    for (const auto& [id, bounds] : cloud.pointerTargetsForAutomation()) {
        const auto tag = id.fromLastOccurrenceOf(".", false, false);
        REQUIRE(rendered.getPixelAt(roundToInt(bounds.getCentreX()), roundToInt(bounds.getY()))
                == cloud.tagAccent(tag));
    }
    const auto reviewPath = SystemStats::getEnvironmentVariable("CYCLE_TAG_CLOUD_REVIEW_PATH", {});
    if (reviewPath.isNotEmpty()) {
        FileOutputStream stream { File(reviewPath) };
        REQUIRE(stream.openedOk());
        REQUIRE(PNGImageFormat().writeImageToStream(rendered, stream));
    }
}
