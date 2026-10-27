#include <catch2/catch_test_macros.hpp>

#include "UI/PresetThumbnailCache.h"
#include <atomic>

using namespace CycleV2;
using namespace juce;

TEST_CASE("Library image cache shares async revision handling and bounded warm reuse",
        "[cycle-v2][browser][thumbnail]") {
    ScopedJuceInitialiser_GUI gui;
    std::atomic<int> loads {};
    const auto pump = [](const std::function<bool()>& ready) {
        for (int attempt = 0; attempt < 80 && !ready(); ++attempt) {
            MessageManager::getInstance()->runDispatchLoopUntil(25);
        }
        REQUIRE(ready());
    };
    LibraryThumbnailCache cache([&](const File&) {
        ++loads;
        return Image(Image::ARGB, 16, 8, true, SoftwareImageType());
    }, 2);
    const File first = File::getSpecialLocation(File::tempDirectory).getChildFile("thumbnail-one");
    const auto second = first.getSiblingFile("thumbnail-two");
    const auto third = first.getSiblingFile("thumbnail-three");
    REQUIRE_FALSE(cache.cachedImageFor(first, 1).isValid());
    REQUIRE(cache.loadCount() == 0);
    cache.imageFor(first, 1);
    cache.imageFor(first, 1);
    cache.imageFor(second, 1);
    pump([&] { return cache.pendingCount() == 0; });
    REQUIRE(loads == 2);
    REQUIRE(cache.cachedImageFor(first, 1).isValid());
    REQUIRE(cache.imageFor(first, 1).isValid());
    REQUIRE(loads == 2);
    cache.imageFor(first, 2);
    REQUIRE_FALSE(cache.cachedImageFor(first, 1).isValid());
    pump([&] { return cache.pendingCount() == 0; });
    REQUIRE(cache.cachedImageFor(first, 2).isValid());
    cache.imageFor(third, 1);
    pump([&] { return cache.pendingCount() == 0; });
    REQUIRE(cache.cachedImageCount() == 2);
    REQUIRE(cache.cachedImageFor(first, 2).isValid());
    REQUIRE_FALSE(cache.cachedImageFor(second, 1).isValid());
    REQUIRE(loads == 4);
}

TEST_CASE("An older async image cannot overwrite a newer file revision",
        "[cycle-v2][browser][thumbnail]") {
    ScopedJuceInitialiser_GUI gui;
    WaitableEvent firstStarted;
    WaitableEvent releaseFirst;
    std::atomic<int> loads {};
    LibraryThumbnailCache cache([&](const File&) {
        const int call = ++loads;
        if (call == 1) {
            firstStarted.signal();
            releaseFirst.wait(2000);
        }
        Image image(Image::ARGB, 4, 4, true, SoftwareImageType());
        Graphics graphics(image);
        graphics.fillAll(call == 1 ? Colours::red : Colours::green);
        return image;
    });
    const auto source = File::getSpecialLocation(File::tempDirectory).getChildFile("thumbnail-revisions");
    int readyCount {};
    cache.setReadyCallback([&] { ++readyCount; });
    cache.imageFor(source, 1);
    REQUIRE(firstStarted.wait(1000));
    cache.imageFor(source, 2);
    for (int attempt = 0; attempt < 40 && cache.pendingCount() != 0; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }
    REQUIRE(cache.cachedImageFor(source, 2).getPixelAt(0, 0) == Colours::green);
    releaseFirst.signal();
    MessageManager::getInstance()->runDispatchLoopUntil(100);
    REQUIRE(cache.cachedImageFor(source, 2).getPixelAt(0, 0) == Colours::green);
    REQUIRE(readyCount == 1);
}

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
