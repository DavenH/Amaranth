#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <Audio/CycleDsp/OscillatorLaneCore.h>

#include "Reference/ReferenceAutomation.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Reference comparison pitch preserves the recorded fundamental across MIDI conventions",
        "[cycle-v2][reference][comparison][pitch]") {
    const auto root = File::getSpecialLocation(File::tempDirectory)
            .getChildFile("cycle-v2-reference-pitch-" + Uuid().toString());
    REQUIRE(root.createDirectory());
    const auto source = root.getChildFile("D293.wav");
    const auto user = root.getChildFile("library");
    constexpr double sampleRate = 48000.;
    const double frequency = MidiMessage::getMidiNoteInHertz(62);
    AudioBuffer<float> buffer(1, (int) sampleRate);
    for (int index = 0; index < buffer.getNumSamples(); ++index) {
        buffer.setSample(0, index, (float) (0.8 * std::sin(
                MathConstants<double>::twoPi * frequency * index / sampleRate)));
    }
    {
        WavAudioFormat format;
        std::unique_ptr<FileOutputStream> stream(source.createOutputStream());
        REQUIRE(stream != nullptr);
        std::unique_ptr<AudioFormatWriter> writer(
                format.createWriterFor(stream.release(), sampleRate, 1, 24, {}, 0));
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
    }
    String error;
    String id;
    double originalPeriod {};
    {
        ReferenceLibrary library({}, user);
        REQUIRE(library.importFile(source, error));
        id = library.selectedId();
        const auto* analysis = library.selectedAnalysis();
        REQUIRE(analysis != nullptr);
        REQUIRE_FALSE(analysis->lowConfidence);
        CHECK(analysis->sample->fundNote == 74);
        REQUIRE(analysis->standardMidiNote() == 62);
        originalPeriod = analysis->sample->getAveragePeriod();
        CHECK(sampleRate / originalPeriod == Catch::Approx(frequency).epsilon(0.002));
        CHECK(sampleRate * CycleDsp::OscillatorLaneCore::angleDelta(
                analysis->standardMidiNote(), 0.f, sampleRate) == Catch::Approx(frequency));
        const auto path = analysis->pitchPath;
        const auto grid = analysis->cyclogram;
        const auto exported = ReferenceAutomation::selectedAnalysis(library, false);
        CHECK((int) exported["schemaVersion"] == 1);
        CHECK((int) exported["effectiveMidiNote"] == 74);
        CHECK((int) exported["standardMidiNote"] == 62);
        CHECK(exported["midiNoteConvention"].toString().contains("plus 12"));
        CHECK((double) exported["pitchPath"][0]["midiNote"]
                - (double) exported["pitchPath"][0]["standardMidiNote"] == Catch::Approx(12.));

        REQUIRE(library.setFundamental(75));
        CHECK(analysis->standardMidiNote() == 63);
        CHECK(analysis->pitchPath == path);
        CHECK(analysis->sample->getAveragePeriod()
                == Catch::Approx(originalPeriod / std::exp2(1. / 12.)).epsilon(0.002));
        REQUIRE(library.undoPitchEdit());
        CHECK(analysis->standardMidiNote() == 62);
        CHECK(analysis->sample->getAveragePeriod() == originalPeriod);
        CHECK(analysis->cyclogram == grid);
        REQUIRE(library.redoPitchEdit());
    }
    {
        ReferenceLibrary library({}, user);
        REQUIRE(library.select(id, error));
        CHECK(library.selectedAnalysis()->sample->fundNote == 75);
        CHECK(library.selectedAnalysis()->standardMidiNote() == 63);
        CHECK(library.selectedAnalysis()->sample->getAveragePeriod()
                == Catch::Approx(originalPeriod / std::exp2(1. / 12.)).epsilon(0.002));
    }
    REQUIRE(root.deleteRecursively());
}

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
    CHECK((int) changed["standardMidiNote"] == (int) original["standardMidiNote"] + 1);
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
