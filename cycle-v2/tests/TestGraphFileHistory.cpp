#include <catch2/catch_test_macros.hpp>

#include "../src/App/GraphFileHistory.h"

using namespace juce;
using namespace CycleV2;

namespace {

PropertiesFile::Options testOptions() {
    PropertiesFile::Options options;
    options.applicationName = "CycleV2GraphFileHistoryTests";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.doNotSave = true;
    options.millisecondsBeforeSaving = -1;
    return options;
}

File contentPreset(const String& name) {
  #if defined(CYCLE_V2_SOURCE_DIR)
    return File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile(name);
  #else
    return File();
  #endif
}

File resource(const String& name) {
  #if defined(CYCLE_V2_SOURCE_DIR)
    return File(CYCLE_V2_SOURCE_DIR).getChildFile("resources").getChildFile(name);
  #else
    return File();
  #endif
}

}

TEST_CASE("Graph file history persists recent files and the last opened directory",
        "[cycle-v2][file-workflow]") {
    ScopedJuceInitialiser_GUI juce;
    PropertiesFile properties(testOptions());
    const File baroque = contentPreset("baroque-flute.cyclegraph");
    const File saved = resource("default.cyclegraph");

    REQUIRE(baroque.existsAsFile());
    REQUIRE(saved.existsAsFile());

    GraphFileHistory history(properties);
    CHECK(history.initialOpenDirectory(baroque.getParentDirectory())
            == baroque.getParentDirectory());
    history.recordOpened(baroque);
    history.recordSaved(saved);

    REQUIRE(history.getNumRecentFiles() == 2);
    CHECK(history.getRecentFile(0) == saved);
    CHECK(history.getRecentFile(1) == baroque);
    CHECK(history.initialOpenDirectory({}) == baroque.getParentDirectory());

    GraphFileHistory restored(properties);
    REQUIRE(restored.getNumRecentFiles() == 2);
    PopupMenu recentMenu;
    CHECK(restored.addRecentMenuItems(recentMenu) == 2);
    CHECK(restored.getRecentFile(0) == saved);
    CHECK(restored.fileForMenuItem(GraphFileHistory::recentMenuBaseId) == saved);
    CHECK(restored.fileForMenuItem(GraphFileHistory::recentMenuBaseId - 1) == File());
    CHECK(restored.initialOpenDirectory({}) == baroque.getParentDirectory());
}
