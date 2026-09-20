#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasChromePalette.h"
#include "UI/InlinePresetBrowser.h"

using namespace CycleV2;
using namespace juce;

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
    std::vector<WorkspaceSidebarTab> tabs;
    InlinePresetBrowser browser(
            { directory },
            [&](const File& file) {
                opened = file;
                return true;
            },
            [&] { ++browseCount; },
            [&] { ++newGuideCount; },
            [&](WorkspaceSidebarTab tab) { tabs.push_back(tab); });
    browser.setBounds(0, 0, 340, 760);

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
    REQUIRE(curves != nullptr);
    REQUIRE(presets != nullptr);
    REQUIRE(search != nullptr);
    REQUIRE(browse != nullptr);
    REQUIRE(all != nullptr);
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

    search->setText("kicker", true);
    for (int attempt = 0; attempt < 12 && browser.visiblePresetCount() != 1; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(browser.visiblePresetCount() == 1);
    REQUIRE(browser.keyPressed(KeyPress(KeyPress::returnKey)));
    REQUIRE(opened.getFileNameWithoutExtension() == "kicker");

    browse->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(40);
    REQUIRE(browseCount == 1);
    REQUIRE(newGuideCount == 0);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
