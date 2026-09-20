#include <catch2/catch_test_macros.hpp>

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
    PresetBrowserPage page(
            { directory },
            [&](const File& file) {
                opened = file;
                return true;
            },
            [&] { ++browseCount; },
            [&] { ++closeCount; });
    page.setBounds(0, 0, 1000, 700);
    auto* search = dynamic_cast<TextEditor*>(page.findChildWithID("presetBrowser.search"));
    auto* viewport = dynamic_cast<Viewport*>(page.findChildWithID("presetBrowser.viewport"));
    auto* grid = viewport == nullptr
            ? nullptr
            : dynamic_cast<PresetCardGrid*>(viewport->getViewedComponent());
    auto* detail = page.findChildWithID("presetBrowser.detail");
    auto* sidebar = page.findChildWithID("presetBrowser.sidebar");
    auto* open = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.open"));
    auto* browse = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.browse"));
    auto* close = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.close"));
    REQUIRE(search != nullptr);
    REQUIRE(grid != nullptr);
    REQUIRE(viewport != nullptr);
    REQUIRE(detail != nullptr);
    REQUIRE(sidebar != nullptr);
    REQUIRE(open != nullptr);
    REQUIRE(browse != nullptr);
    REQUIRE(close != nullptr);
    for (int attempt = 0; attempt < 20 && grid->visibleCount() == 0; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(100);
    }
    REQUIRE(grid->visibleCount() > 1);
    REQUIRE(viewport->getWidth() > sidebar->getWidth());
    REQUIRE(detail->getWidth() > 200);
    REQUIRE(detail->getBounds().contains(open->getBounds()));
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
