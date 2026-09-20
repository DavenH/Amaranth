#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasChromePalette.h"
#include "UI/InlinePresetBrowser.h"

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
    int newGuideCount {};
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
            [&] { ++newGuideCount; },
            [&](WorkspaceSidebarTab tab) { tabs.push_back(tab); },
            [&](const File& file) {
                deleted = file;
                return true;
            },
            [&](const String& name, std::function<void(bool)> completion) {
                deleteConfirmationName = name;
                finishDeleteConfirmation = std::move(completion);
            });
    browser.setBounds(0, 0, 272, 760);

    auto* curves = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.curves"));
    auto* presets = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.presets"));
    auto* search = dynamic_cast<TextEditor*>(
            browser.findChildWithID("workspace.sidebar.search"));
    auto* browse = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.browse"));
    auto* all = dynamic_cast<Button*>(
            browser.findChildWithID("workspace.sidebar.all"));
    auto* list = findDescendantWithID(browser, "workspace.sidebar.list");
    auto* remove = dynamic_cast<Button*>(
            findDescendantWithID(browser, "workspace.sidebar.delete"));
    REQUIRE(curves != nullptr);
    REQUIRE(presets != nullptr);
    REQUIRE(search != nullptr);
    REQUIRE(browse != nullptr);
    REQUIRE(all != nullptr);
    REQUIRE(list != nullptr);
    REQUIRE(remove != nullptr);
    REQUIRE(search->getFont().getHeight() >= 14.f);
    REQUIRE(all->getToggleState());
    REQUIRE(all->findColour(TextButton::textColourOnId)
            == CanvasChromePalette::canvasBackground);
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
    REQUIRE(list->getHeight() == 234 + browser.visiblePresetCount() * 78);

    search->setText("kicker", true);
    for (int attempt = 0; attempt < 12 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);
    REQUIRE(browser.keyPressed(KeyPress(KeyPress::returnKey)));
    REQUIRE(opened.getFileNameWithoutExtension() == "kicker");

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
    REQUIRE(newGuideCount == 0);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
