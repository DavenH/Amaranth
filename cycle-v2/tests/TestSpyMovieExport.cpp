#include <catch2/catch_test_macros.hpp>

#include "App/SpyMovieExport.h"
#include "UI/SpyMovieExportWindow.h"

using namespace CycleV2;
using namespace juce;

TEST_CASE("Native movie settings reject destructive and unsupported jobs",
        "[cycle-v2][movie-export]") {
    ScopedJuceInitialiser_GUI gui;
    const auto directory = File::getSpecialLocation(File::tempDirectory).getChildFile(Uuid().toString());
    SpyMovieExportSettings settings;
    settings.outputDirectory = directory;
    settings.ffmpeg = File::getSpecialLocation(File::currentExecutableFile);
    PresetMidiSequence sequence;
    sequence.notes = {{60, 100, 0.0, 0.5}};
    REQUIRE(settings.validate(sequence).wasOk());
    SECTION("Existing folders are protected") {
        REQUIRE(directory.createDirectory());
        REQUIRE(settings.validate(sequence).failed());
        directory.deleteRecursively();
    }
    SECTION("ffmpeg must exist") {
        settings.ffmpeg = directory.getChildFile("missing-ffmpeg");
        REQUIRE(settings.validate(sequence).failed());
    }
    SECTION("Frame rate and size are bounded") {
        settings.fps = 0;
        REQUIRE(settings.validate(sequence).failed());
        settings.fps = 30;
        settings.width = 1919;
        REQUIRE(settings.validate(sequence).failed());
    }
    SECTION("Movie audio requires notes") {
        sequence.notes.clear();
        REQUIRE(settings.validate(sequence).failed());
    }
    SECTION("The release tail respects the offline duration limit") {
        sequence.durationSeconds = 120;
        REQUIRE(settings.validate(sequence).failed());
    }
    SECTION("Unknown palettes are rejected") {
        settings.gradient = "not-a-gradient";
        REQUIRE(settings.validate(sequence).failed());
    }
    SECTION("The job freezes unsaved preset and phrase content") {
        const auto job = settings.toJSON("unsaved-preset-snapshot", sequence);
        sequence.notes[0].pitch = 72;
        REQUIRE(job["preset"].toString() == "unsaved-preset-snapshot");
        REQUIRE(PresetPresentationCodec::readSequenceJSON(job["sequence"])->notes[0].pitch == 60);
    }
}

TEST_CASE("Movie settings are nonmodal so editing can continue during export",
        "[cycle-v2][movie-export][ui]") {
    ScopedJuceInitialiser_GUI gui;
    Component parent;
    parent.setBounds(100, 100, 1280, 760);
    SpyMovieExportWindow window(parent, [] { return std::make_pair(String("snapshot"), PresetMidiSequence {}); });
    REQUIRE(window.isVisible());
    REQUIRE_FALSE(window.isCurrentlyModal(false));
    REQUIRE(window.getContentComponent()->getHeight() >= 480);
    window.closeButtonPressed();
    REQUIRE_FALSE(window.isVisible());
}
