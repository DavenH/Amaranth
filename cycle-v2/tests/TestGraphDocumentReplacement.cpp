#include <catch2/catch_test_macros.hpp>

#include "App/GraphDocumentReplacement.h"

using namespace juce;
using namespace CycleV2;

TEST_CASE("Graph replacement protects dirty documents until the user decides",
        "[cycle-v2][file-workflow]") {
    File replacedFile;
    int replacementCount = 0;
    GraphDocumentReplacement replacement([&](const File& file) {
        replacedFile = file;
        ++replacementCount;
        return true;
    });
    const File first("/tmp/cycle-v2-first.cyclegraph");
    const File second("/tmp/cycle-v2-second.cyclegraph");

    CHECK(replacement.request(first, false)
            == GraphDocumentReplacement::RequestResult::Replaced);
    CHECK(replacedFile == first);
    CHECK(replacementCount == 1);

    CHECK(replacement.request(second, true)
            == GraphDocumentReplacement::RequestResult::DecisionRequired);
    CHECK(replacement.isPending());
    CHECK(replacement.request(first, true)
            == GraphDocumentReplacement::RequestResult::Rejected);
    CHECK(replacementCount == 1);

    CHECK(replacement.resolve(GraphDocumentReplacement::Decision::Cancel)
            == GraphDocumentReplacement::Resolution::Canceled);
    CHECK_FALSE(replacement.isPending());
    CHECK(replacedFile == first);
    CHECK(replacementCount == 1);

    REQUIRE(replacement.request(second, true)
            == GraphDocumentReplacement::RequestResult::DecisionRequired);
    CHECK(replacement.resolve(GraphDocumentReplacement::Decision::Discard)
            == GraphDocumentReplacement::Resolution::Replaced);
    CHECK(replacedFile == second);
    CHECK(replacementCount == 2);
}

TEST_CASE("Graph replacement continues only after a successful save",
        "[cycle-v2][file-workflow]") {
    File replacedFile;
    GraphDocumentReplacement replacement([&](const File& file) {
        replacedFile = file;
        return true;
    });
    const File target("/tmp/cycle-v2-after-save.cyclegraph");

    REQUIRE(replacement.request(target, true)
            == GraphDocumentReplacement::RequestResult::DecisionRequired);
    CHECK(replacement.resolve(GraphDocumentReplacement::Decision::Save)
            == GraphDocumentReplacement::Resolution::SaveRequired);
    CHECK(replacement.resolve(GraphDocumentReplacement::Decision::Discard)
            == GraphDocumentReplacement::Resolution::Ignored);
    CHECK(replacedFile == File());

    CHECK(replacement.completeSave(false)
            == GraphDocumentReplacement::Resolution::Failed);
    CHECK_FALSE(replacement.isPending());
    CHECK(replacedFile == File());

    REQUIRE(replacement.request(target, true)
            == GraphDocumentReplacement::RequestResult::DecisionRequired);
    REQUIRE(replacement.resolve(GraphDocumentReplacement::Decision::Save)
            == GraphDocumentReplacement::Resolution::SaveRequired);
    CHECK(replacement.completeSave(true)
            == GraphDocumentReplacement::Resolution::Replaced);
    CHECK_FALSE(replacement.isPending());
    CHECK(replacedFile == target);
}
