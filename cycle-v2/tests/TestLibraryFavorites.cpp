#include <catch2/catch_test_macros.hpp>

#include "App/LibraryFavorites.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Library favorites persist independently of preset and pattern documents",
        "[cycle-v2][library][favorites]") {
    ScopedJuceInitialiser_GUI gui;
    PropertiesFile::Options options;
    options.applicationName = "CycleV2FavoritesTest-" + Uuid().toString();
    options.folderName = "Amaranth Audio/Cycle V2 Tests";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = -1;

    const File originalFactory("/tmp/cycle-v2-factory-a/presets");
    const File movedFactory("/tmp/cycle-v2-factory-b/presets");
    const File userPreset("/tmp/cycle-v2-user/favorite.cyclegraph");
    File settingsFile;
    {
        PropertiesFile properties(options);
        settingsFile = properties.getFile();
        LibraryFavorites favorites(properties, originalFactory);
        REQUIRE_FALSE(favorites.isPresetFavorite(originalFactory.getChildFile("bass.cyclegraph")));
        REQUIRE(favorites.togglePreset(originalFactory.getChildFile("bass.cyclegraph")));
        REQUIRE(favorites.togglePreset(userPreset));
        REQUIRE(favorites.togglePattern("factory-basic-bass"));
        REQUIRE(properties.saveIfNeeded());
    }
    {
        PropertiesFile properties(options);
        LibraryFavorites favorites(properties, movedFactory);
        REQUIRE(favorites.isPresetFavorite(movedFactory.getChildFile("bass.cyclegraph")));
        REQUIRE(favorites.isPresetFavorite(userPreset));
        REQUIRE(favorites.isPatternFavorite("factory-basic-bass"));
        REQUIRE_FALSE(favorites.isPatternFavorite("factory-basic-lead"));
        REQUIRE_FALSE(favorites.togglePattern("factory-basic-bass"));
        REQUIRE_FALSE(favorites.isPatternFavorite("factory-basic-bass"));
    }
    REQUIRE(settingsFile.deleteFile());
}
