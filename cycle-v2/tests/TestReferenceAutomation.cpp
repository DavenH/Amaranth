#include <catch2/catch_test_macros.hpp>

#include "Reference/ReferenceAutomation.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Reference automation exports authoritative corrected analysis without selection changes",
        "[cycle-v2][reference][automation]") {
    const auto factory = File(CYCLE_V2_SOURCE_DIR).getChildFile("content/references");
    const auto user = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-export-" + Uuid().toString());
    ReferenceLibrary library(factory, user);
    REQUIRE_FALSE(ReferenceAutomation::selectedAnalysis(library, true).isObject());
    const auto catalogue = ReferenceAutomation::catalogue(library);
    REQUIRE(catalogue.getProperty("references", {}).size() >= 2);
    REQUIRE(library.selectedId().isEmpty());
    String error;
    REQUIRE(library.select("factory:sax-growl", error));
    const auto original = ReferenceAutomation::selectedAnalysis(library, false);
    REQUIRE_FALSE(original["cyclogram"].getDynamicObject()->hasProperty("values"));
    const auto selected = library.selectedId();
    const auto* analysis = library.selectedAnalysis();
    REQUIRE(library.setFundamental(analysis->sample->fundNote + 1));
    const auto changed = ReferenceAutomation::selectedAnalysis(library, true);
    CHECK(library.selectedId() == selected);
    CHECK((int) changed["effectiveMidiNote"] == (int) original["effectiveMidiNote"] + 1);
    CHECK((float) changed["trackingInsecurity"] == analysis->pitchInsecurity);
    CHECK((bool) changed["trackingUsedFallback"] == (analysis->pitchInsecurity > 50.f));
    const auto values = changed["cyclogram"]["values"];
    REQUIRE(values.size() == (int) analysis->cyclogram.size());
    for (int index = 0; index < values.size(); ++index) {
        REQUIRE((float) values[index] == analysis->cyclogram[(size_t) index]);
    }
    const auto points = changed["pitchPath"];
    REQUIRE(points.size() == (int) analysis->pitchPath.size());
    for (int index = 0; index < points.size(); ++index) {
        CHECK((float) points[index]["time"] == analysis->pitchPath[(size_t) index].x);
        CHECK((float) points[index]["pitch"] == analysis->pitchPath[(size_t) index].y);
    }
    const auto frames = changed["pitchFrames"];
    REQUIRE(frames.size() == (int) analysis->sample->periods.size());
    CHECK((double) frames[1]["timeSeconds"]
            == (double) analysis->sample->periods[1].sampleOffset / analysis->sample->samplerate);
    REQUIRE(library.undoPitchEdit());
    CHECK((int) ReferenceAutomation::selectedAnalysis(library, false)["effectiveMidiNote"]
            == (int) original["effectiveMidiNote"]);
    user.deleteRecursively();
}

TEST_CASE("Reference automation preserves insecure pitch and imported record identity",
        "[cycle-v2][reference][automation]") {
    const auto source = File(CYCLE_V2_SOURCE_DIR).getChildFile("tests/fixtures/audio/noise.wav");
    const auto user = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-confidence-" + Uuid().toString());
    String id;
    String error;
    {
        ReferenceLibrary library({}, user);
        REQUIRE(library.importFile(source, error));
        id = library.selectedId();
        const auto value = ReferenceAutomation::selectedAnalysis(library, false);
        CHECK(value["referenceId"].toString() == id);
        CHECK((bool) value["lowConfidence"]);
        CHECK((float) value["trackingInsecurity"] > 30.f);
        CHECK(File(value["audioPath"].toString()).getParentDirectory() == user);
    }
    {
        ReferenceLibrary library({}, user);
        const auto catalogue = ReferenceAutomation::catalogue(library);
        REQUIRE(catalogue["references"].size() == 1);
        CHECK(catalogue["references"][0]["referenceId"].toString() == id);
        CHECK_FALSE((bool) catalogue["references"][0]["open"]);
        REQUIRE(library.select(id, error));
        CHECK((bool) ReferenceAutomation::selectedAnalysis(library, false)["lowConfidence"]);
    }
    user.deleteRecursively();
}
