#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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

TEST_CASE("Open references remain session-owned while library records reload",
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
        REQUIRE(library.selectedId() == firstId);
        REQUIRE(library.setSpectralView(firstId, true));
        REQUIRE(library.spectralViewFor(firstId));
        REQUIRE_FALSE(library.spectralViewFor(secondId));
        REQUIRE(library.select(secondId, error));
        REQUIRE(library.select(firstId, error));
        REQUIRE(library.spectralViewFor(firstId));
        REQUIRE(library.setFundamental(library.selectedAnalysis()->sample->fundNote + 1));
        REQUIRE(library.undoPitchEdit());
        REQUIRE(library.spectralViewFor(firstId));
    }
    root.getChildFile("library.json").replaceWithText(
            "{\"selected\":\"" + firstId + "\",\"open\":[\""
                    + firstId + "\",\"" + secondId + "\"]}");
    {
        ReferenceLibrary library({}, root);
        REQUIRE(library.records().size() == 2);
        REQUIRE(library.openReferenceIds().empty());
        REQUIRE(library.selectedId().isEmpty());
        REQUIRE(library.analysisFor(secondId) == nullptr);
        REQUIRE(library.select(secondId, error));
        REQUIRE(library.openReferenceIds() == std::vector<String> { secondId });
    }
    root.deleteRecursively();
}

TEST_CASE("Reference loading trims to five seconds before peak normalization and analysis",
        "[cycle-v2][reference][resolution]") {
    const File source = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-duration-" + Uuid().toString() + ".wav");
    for (int rate : { 44100, 96000 }) {
        AudioBuffer<float> audio(2, rate * 6);
        for (int index = 0; index < audio.getNumSamples(); ++index) {
            const float value = (float) std::sin(juce::MathConstants<double>::twoPi * 220 * index / rate);
            const float gain = index < 5 * rate ? 0.1f : 0.9f;
            audio.setSample(0, index, value * gain * 0.5f);
            audio.setSample(1, index, value * gain);
        }
        source.deleteFile();
        {
            WavAudioFormat format;
            std::unique_ptr<FileOutputStream> stream(source.createOutputStream());
            REQUIRE(stream != nullptr);
            std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(), rate, 2, 24, {}, 0));
            REQUIRE(writer != nullptr);
            REQUIRE(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()));
        }
        const auto originalSize = source.getSize();
        const auto analysis = ReferenceSampleAnalyzer::analyze(source);
        REQUIRE(analysis.succeeded());
        REQUIRE(analysis.sample->size() == rate * 5);
        REQUIRE(analysis.sample->audio.right.max() == Catch::Approx(1.f));
        REQUIRE(analysis.sample->audio.left.max() == Catch::Approx(0.5f).margin(1e-5));
        REQUIRE(analysis.columns == 1024);
        REQUIRE(analysis.rows <= 1024);
        REQUIRE(source.getSize() == originalSize);
    }
    source.deleteFile();
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
