#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("Sidebar tag chips toggle an inclusive filter",
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
    REQUIRE(cloud.matches({ "Pad" }));
    REQUIRE_FALSE(cloud.matches({ "Keys" }));
    REQUIRE(changes == 2);

    cloud.setTags({ "Bass", "Pad", "Texture" });
    REQUIRE(cloud.selectedTags().size() == 2);
    REQUIRE(clickTag(cloud, "Bass"));
    REQUIRE(clickTag(cloud, "Pad"));
    REQUIRE(cloud.matches({ "Keys" }));
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
            browser.findChildWithID("workspace.sidebar.search"));
    auto* browse = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.browse"));
    auto* create = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.presetNew"));
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
    REQUIRE(tagCloud != nullptr);
    REQUIRE(list != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(remove != nullptr);
    REQUIRE(search->getFont().getHeight() >= 14.f);
    REQUIRE(search->getHeight() == 30);
    REQUIRE(create->getHeight() == search->getHeight());
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
    presets->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browser.activeTab() == WorkspaceSidebarTab::Presets);
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
