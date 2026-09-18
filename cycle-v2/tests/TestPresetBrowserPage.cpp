#include <catch2/catch_test_macros.hpp>

#include "UI/PresetBrowserPage.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Preset browser searches local presets and opens the selected file",
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
    auto* list = dynamic_cast<ListBox*>(page.findChildWithID("presetBrowser.list"));
    auto* open = dynamic_cast<Button*>(page.findChildWithID("presetBrowser.open"));
    REQUIRE(search != nullptr);
    REQUIRE(list != nullptr);
    REQUIRE(open != nullptr);
    REQUIRE(list->getModel()->getNumRows() > 1);

    search->setText("kicker", true);
    MessageManager::getInstance()->runDispatchLoopUntil(50);
    REQUIRE(list->getModel()->getNumRows() == 1);
    open->triggerClick();
    MessageManager::getInstance()->runDispatchLoopUntil(50);
    REQUIRE(opened.getFileName() == "kicker.cyclegraph");
    REQUIRE(closeCount == 1);
    REQUIRE(browseCount == 0);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
