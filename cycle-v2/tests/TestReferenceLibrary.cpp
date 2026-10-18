#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "Reference/ReferenceLibrary.h"

using namespace CycleV2;
using namespace juce;

namespace {

File referenceWave(const String& name) {
    return File(CYCLE_V2_SOURCE_DIR).getParentDirectory()
            .getChildFile("cycle/content/wav").getChildFile(name);
}

File audioFixture(const String& name) {
    return File(CYCLE_V2_SOURCE_DIR).getChildFile("tests/fixtures/audio")
            .getChildFile(name);
}

}

TEST_CASE("Reference analysis produces a period-folded grid from Cycle content",
        "[cycle-v2][reference][analysis]") {
    const File source = referenceWave("sax-growl.wav");
    REQUIRE(source.existsAsFile());
    auto analysis = ReferenceSampleAnalyzer::analyze(source, 80, 128);
    REQUIRE(analysis.succeeded());
    REQUIRE(analysis.sample->fundNote >= 0);
    REQUIRE(analysis.sample->fundNote <= 127);
    REQUIRE(analysis.pitchPath.size() >= 2);
    REQUIRE(analysis.cyclogram.size() == 80 * 128);
    REQUIRE(std::any_of(analysis.cyclogram.begin(), analysis.cyclogram.end(),
            [](float value) { return value != 0.f; }));

    const var sidecar = JSON::parse(source.withFileExtension(".env").loadFileAsString());
    REQUIRE(sidecar.isObject());
    CHECK(std::abs(analysis.sample->fundNote
            - (int) sidecar.getProperty("midiNote", -1)) <= 2);
}

TEST_CASE("Imported references reload with pitch edits independently of presets",
        "[cycle-v2][reference][library]") {
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-test-" + Uuid().toString());
    root.createDirectory();
    String error;
    String id;
    int originalNote {};
    size_t editedPointIndex {};
    float editedPointPitch {};
    {
        ReferenceLibrary library({}, root);
        REQUIRE(library.importFile(referenceWave("sax-growl.wav"), error));
        id = library.selectedId();
        originalNote = library.selectedAnalysis()->sample->fundNote;
        const auto originalCyclogram = library.selectedAnalysis()->cyclogram;
        REQUIRE(library.setFundamental(originalNote + 1));
        REQUIRE(library.selectedAnalysis()->sample->fundNote == originalNote + 1);
        REQUIRE(library.selectedAnalysis()->cyclogram != originalCyclogram);
        REQUIRE(library.undoPitchEdit());
        REQUIRE(library.selectedAnalysis()->sample->fundNote == originalNote);
        REQUIRE(library.redoPitchEdit());
        REQUIRE(library.selectedAnalysis()->sample->fundNote == originalNote + 1);
        auto path = library.selectedAnalysis()->pitchPath;
        const auto beforePathEdit = library.selectedAnalysis()->cyclogram;
        const size_t middle = path.size() / 2;
        path[middle].y = juce::jlimit(0.f, 1.f,
                path[middle].y < 0.5f ? path[middle].y + 0.2f
                        : path[middle].y - 0.2f);
        REQUIRE(std::adjacent_find(path.begin(), path.end(),
                [](const auto& left, const auto& right) {
                    return left.x == right.x;
                }) != path.end());
        REQUIRE(library.replacePitchPath(path));
        REQUIRE(library.selectedAnalysis()->pitchPath == path);
        REQUIRE(library.selectedAnalysis()->cyclogram != beforePathEdit);
        REQUIRE(library.undoPitchEdit());
        REQUIRE(library.selectedAnalysis()->cyclogram == beforePathEdit);
        REQUIRE(library.redoPitchEdit());
        REQUIRE(library.selectedAnalysis()->pitchPath == path);
        editedPointIndex = middle;
        editedPointPitch = path[middle].y;
    }
    {
        ReferenceLibrary library({}, root);
        REQUIRE(library.records().size() == 1);
        REQUIRE(library.select(id, error));
        REQUIRE(library.selectedAnalysis()->sample->fundNote == originalNote + 1);
        REQUIRE(library.selectedAnalysis()->pitchPath.size() >= 2);
        REQUIRE(library.selectedAnalysis()->pitchPath[editedPointIndex].y
                == editedPointPitch);
    }
    root.deleteRecursively();
}

TEST_CASE("Open references persist in order while selection changes",
        "[cycle-v2][reference][library]") {
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-open-" + Uuid().toString());
    root.createDirectory();
    String error;
    String firstId;
    String secondId;
    {
        ReferenceLibrary library({}, root);
        REQUIRE(library.importFile(referenceWave("sax-growl.wav"), error));
        firstId = library.selectedId();
        REQUIRE(library.importFile(referenceWave("sine500.wav"), error));
        secondId = library.selectedId();
        REQUIRE(library.openReferenceIds() == std::vector<String> { firstId, secondId });
        REQUIRE(library.select(firstId, error));
    }
    {
        ReferenceLibrary library({}, root);
        REQUIRE(library.openReferenceIds() == std::vector<String> { firstId, secondId });
        REQUIRE(library.selectedId() == firstId);
        REQUIRE(library.analysisFor(secondId) != nullptr);
        REQUIRE(library.close(firstId));
        REQUIRE(library.selectedId() == secondId);
    }
    root.deleteRecursively();
}

TEST_CASE("Reference import rejects missing and unsupported audio",
        "[cycle-v2][reference][decode]") {
    const File root = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-invalid-" + Uuid().toString());
    root.createDirectory();
    const File invalid = root.getChildFile("invalid.xyz");
    invalid.replaceWithText("not audio");
    ReferenceLibrary library({}, root);
    String error;
    REQUIRE_FALSE(library.importFile(invalid, error));
    REQUIRE(library.records().empty());
    REQUIRE_FALSE(ReferenceSampleAnalyzer::analyze(root.getChildFile("missing.wav")).succeeded());
    REQUIRE_FALSE(ReferenceSampleAnalyzer::analyze(audioFixture("short.wav")).succeeded());
    REQUIRE_FALSE(ReferenceSampleAnalyzer::analyze(audioFixture("silent.wav")).succeeded());
    root.deleteRecursively();
}

TEST_CASE("Low-confidence periodic analysis exposes a pitch correction target",
        "[cycle-v2][reference][analysis]") {
    const auto analysis = ReferenceSampleAnalyzer::analyze(audioFixture("noise.wav"));
    REQUIRE(analysis.succeeded());
    REQUIRE(analysis.lowConfidence);
    REQUIRE(analysis.pitchInsecurity > 30.f);
    REQUIRE(analysis.pitchPath.size() >= 2);
}

TEST_CASE("PitchedSample decodes all supported comparison formats",
        "[cycle-v2][reference][decode]") {
    const StringArray files {
            "sine500.aif",
            "sine500.aiff",
            "sine500.mp3",
            "sine500.flac",
            "sine500.ogg"
    };
    for (const auto& name : files) {
        PitchedSample sample;
        INFO(name.toStdString());
        REQUIRE(sample.load(audioFixture(name).getFullPathName()) == 0);
        REQUIRE(sample.size() >= 2048);
    }
}
