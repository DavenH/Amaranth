#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "UI/PresetLibraryIndex.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Preset library publishes only the newest coalesced search",
        "[cycle-v2][preset][browser][async]") {
    ScopedJuceInitialiser_GUI juce;
    const File directory = File("/private/tmp")
            .getNonexistentChildFile("cycle-v2-preset-index", {}, false);
    REQUIRE(directory.createDirectory().wasOk());

    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "output", {}));
    REQUIRE(directory.getChildFile("Acidic 2.cyclegraph").replaceWithText(
            GraphSerializer().toJsonString(graph)));
    REQUIRE(directory.getChildFile("Soft Pad.cyclegraph").replaceWithText(
            GraphSerializer().toJsonString(graph)));

    struct Publication {
        String names;
        bool metadataReady {};
    };
    std::vector<Publication> publications;
    PresetLibraryIndex index({ directory }, [&](const auto& records, const auto& indices) {
        StringArray names;
        for (const int recordIndex : indices) {
            names.add(records[(size_t) recordIndex].name);
        }
        publications.push_back({
                names.joinIntoString(","),
                std::all_of(records.begin(), records.end(), [](const auto& record) {
                    return record.metadataReady;
                })
        });
    });
    index.start();
    index.setQuery("a");
    index.setQuery("acid");
    index.setQuery("acidic 2");

    for (int attempt = 0; attempt < 20 && publications.size() < 2; ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    REQUIRE(publications.size() >= 2);
    REQUIRE(publications.front().names == "Acidic 2");
    REQUIRE_FALSE(publications.front().metadataReady);
    REQUIRE(publications.back().names == "Acidic 2");
    REQUIRE(publications.back().metadataReady);
    REQUIRE(index.publishedGeneration() == 4);

    REQUIRE(directory.deleteRecursively());
}

TEST_CASE("Preset library prefers the factory copy of duplicate preset names",
        "[cycle-v2][preset][browser][deduplication]") {
    ScopedJuceInitialiser_GUI juce;
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-preset-deduplication", {}, false);
    const File factory = root.getChildFile("factory");
    const File user = root.getChildFile("user");
    REQUIRE(factory.createDirectory().wasOk());
    REQUIRE(user.createDirectory().wasOk());
    REQUIRE(factory.getChildFile("Lead.cyclegraph").replaceWithText("{}"));
    REQUIRE(user.getChildFile("lead.cyclegraph").replaceWithText("{}"));

    std::vector<PresetLibraryRecord> published;
    PresetLibraryIndex index({ factory, user }, [&](const auto& records, const auto&) {
        published = records;
    });
    index.start();
    for (int attempt = 0; attempt < 20
            && (published.empty() || !published.front().metadataReady); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    REQUIRE(published.size() == 1);
    REQUIRE(published.front().file.getParentDirectory() == factory);
    REQUIRE(root.deleteRecursively());
}

TEST_CASE("Preset library retains differently authored presets with the same name",
        "[cycle-v2][preset][browser][deduplication]") {
    ScopedJuceInitialiser_GUI juce;
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-preset-name-collision", {}, false);
    const File factory = root.getChildFile("factory");
    const File user = root.getChildFile("user");
    REQUIRE(factory.createDirectory().wasOk());
    REQUIRE(user.createDirectory().wasOk());
    REQUIRE(factory.getChildFile("Lead.cyclegraph").replaceWithText("{}"));
    REQUIRE(user.getChildFile("lead.cyclegraph").replaceWithText("{\"user\":true}"));

    std::vector<PresetLibraryRecord> published;
    PresetLibraryIndex index({ factory, user }, [&](const auto& records, const auto&) {
        published = records;
    });
    index.start();
    for (int attempt = 0; attempt < 20
            && (published.size() < 2 || !published.front().metadataReady); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    REQUIRE(published.size() == 2);
    REQUIRE(root.deleteRecursively());
}

TEST_CASE("Preset library ignores a second checkout's factory catalog",
        "[cycle-v2][preset][browser][deduplication]") {
    ScopedJuceInitialiser_GUI juce;
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-preset-checkouts", {}, false);
    const File factory = root.getChildFile("current/cycle-v2/content/presets");
    const File otherCheckout = root.getChildFile("other/cycle-v2/content/presets");
    const File user = root.getChildFile("user");
    REQUIRE(factory.createDirectory().wasOk());
    REQUIRE(otherCheckout.createDirectory().wasOk());
    REQUIRE(user.createDirectory().wasOk());
    REQUIRE(factory.getChildFile("Lead.cyclegraph").replaceWithText("{}"));
    REQUIRE(otherCheckout.getChildFile("Lead.cyclegraph")
            .replaceWithText("{\"edited\":true}"));
    REQUIRE(otherCheckout.getChildFile("Other.cyclegraph")
            .replaceWithText("{}"));
    REQUIRE(user.getChildFile("Personal.cyclegraph").replaceWithText("{}"));

    std::vector<PresetLibraryRecord> published;
    PresetLibraryIndex index({ factory, otherCheckout, user },
            [&](const auto& records, const auto&) { published = records; });
    index.start();
    for (int attempt = 0; attempt < 20
            && (published.size() < 2 || !published.front().metadataReady); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    REQUIRE(published.size() == 2);
    REQUIRE(published[0].file.getParentDirectory() == factory);
    REQUIRE(published[1].file.getParentDirectory() == user);
    REQUIRE(root.deleteRecursively());
}

TEST_CASE("Preset library does not rescan a nested history directory",
        "[cycle-v2][preset][browser][directories]") {
    ScopedJuceInitialiser_GUI juce;
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getNonexistentChildFile("cycle-v2-preset-nested-history", {}, false);
    const File old = root.getChildFile("old");
    REQUIRE(old.createDirectory().wasOk());
    REQUIRE(root.getChildFile("Current.cyclegraph").replaceWithText("{}"));
    REQUIRE(old.getChildFile("Archived.cyclegraph").replaceWithText("{}"));

    std::vector<PresetLibraryRecord> published;
    PresetLibraryIndex index({ root, old }, [&](const auto& records, const auto&) {
        published = records;
    });
    index.start();
    for (int attempt = 0; attempt < 20
            && (published.empty() || !published.front().metadataReady); ++attempt) {
        MessageManager::getInstance()->runDispatchLoopUntil(25);
    }

    REQUIRE(published.size() == 1);
    REQUIRE(published.front().name == "Current");
    REQUIRE(root.deleteRecursively());
}
