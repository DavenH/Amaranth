#include <catch2/catch_test_macros.hpp>

#include <array>

#include "UI/InlinePresetBrowser.h"
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
            == 20 + browser.visiblePresetCount() * SidebarMediaRow::height);
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
    REQUIRE(browser.visiblePresetCount() == 4);
    REQUIRE(clickTag(*tagCloud, "Bass"));
    REQUIRE(browser.visiblePresetCount() == 1);
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

    const Point<float> position { 28.f, 24.f };
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
