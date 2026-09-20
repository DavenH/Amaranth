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
