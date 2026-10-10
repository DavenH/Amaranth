#include <catch2/catch_test_macros.hpp>

#include "UI/PresetThumbnailCache.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Preset thumbnails decode asynchronously and cache by file revision",
        "[cycle-v2][preset][browser][thumbnail]") {
    ScopedJuceInitialiser_GUI juce;
  #if defined(CYCLE_V2_SOURCE_DIR)
    PresetLibraryRecord record;
    record.file = File(CYCLE_V2_SOURCE_DIR)
            .getChildFile("content")
            .getChildFile("presets")
            .getChildFile("kicker.cyclegraph");
    REQUIRE(record.file.existsAsFile());
    record.modificationTime = record.file.getLastModificationTime().toMilliseconds();

    int readyCount {};
    PresetThumbnailCache cache;
    cache.setReadyCallback([&] { ++readyCount; });

    REQUIRE_FALSE(cache.imageFor(record).isValid());
    REQUIRE(cache.pendingCount() == 1);
    REQUIRE(cache.cachedImageCount() == 0);

    for (int attempt = 0; attempt < 40 && cache.cachedImageCount() == 0; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    const Image image = cache.imageFor(record);
    REQUIRE(image.isValid());
    REQUIRE(image.getWidth() == 320);
    REQUIRE(image.getHeight() == 180);
    REQUIRE(cache.pendingCount() == 0);
    REQUIRE(cache.cachedImageCount() == 1);
    REQUIRE(readyCount == 1);
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
