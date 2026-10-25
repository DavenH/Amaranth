#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "UI/ReferenceWaveformPreview.h"
#include "UI/LibraryIconRenderer.h"

using namespace CycleV2;

TEST_CASE("Waveform density counts trace passages instead of filling the amplitude range",
        "[cycle-v2][reference][waveform][density]") {
    float signal[] { -1.f, 1.f, -1.f, 1.f, -1.f };
    const auto density = referenceWaveformDensity(Buffer<float>(signal, 5), 1, 5);
    REQUIRE(density.size() == 5);
    for (float count : density) {
        REQUIRE(count == 4.f);
    }
    float repeated[] { -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f, 1.f, -1.f };
    const auto twice = referenceWaveformDensity(Buffer<float>(repeated, 9), 1, 5);
    REQUIRE(twice[2] == 2.f * density[2]);

    float flat[] { 0.5f, 0.5f, 0.5f };
    const auto flatDensity = referenceWaveformDensity(Buffer<float>(flat, 3), 16, 9);
    for (int column = 0; column < 16; ++column) {
        REQUIRE(flatDensity[(size_t) column * 9 + 2] > 0.f);
        REQUIRE(flatDensity[(size_t) column * 9 + 4] == 0.f);
        REQUIRE(flatDensity[(size_t) column * 9] == 0.f);
    }
}

TEST_CASE("Waveform heatmaps preserve silence stereo cancellation and quiet-source contrast",
        "[cycle-v2][reference][waveform][density]") {
    juce::AudioBuffer<float> audio(2, 4);
    audio.clear();
    const auto silent = referenceWaveformHeatmap(StereoBuffer(audio), 32, 16);
    REQUIRE(silent.isValid());
    REQUIRE(silent.getPixelAt(15, 8).getAlpha() == 0);
    const float left[] { 0.001f, -0.002f, 0.003f, -0.004f };
    audio.copyFrom(0, 0, left, 4);
    audio.copyFrom(1, 0, left, 4);
    const auto quiet = referenceWaveformHeatmap(StereoBuffer(audio), 32, 16);
    audio.applyGain(100.f);
    const auto loud = referenceWaveformHeatmap(StereoBuffer(audio), 32, 16);
    int nonempty {};
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 16; ++y) {
            REQUIRE(quiet.getPixelAt(x, y) == loud.getPixelAt(x, y));
            nonempty += quiet.getPixelAt(x, y).getAlpha() > 0 ? 1 : 0;
        }
    }
    REQUIRE(nonempty > 0);
    audio.applyGain(1, 0, 4, -1.f);
    const auto cancelled = referenceWaveformHeatmap(StereoBuffer(audio), 32, 16);
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 16; ++y) {
            REQUIRE(cancelled.getPixelAt(x, y).getAlpha() == 0);
        }
    }
}

TEST_CASE("Long asset previews visualize exactly their first five seconds",
        "[cycle-v2][reference][waveform][density]") {
    constexpr int rate = 44100;
    juce::AudioBuffer<float> audio(1, 6 * rate);
    for (int index = 0; index < audio.getNumSamples(); ++index) {
        audio.setSample(0, index, index < 2 * rate ? 0.25f : index < 5 * rate ? -0.5f : 1.f);
    }
    const juce::TemporaryFile temporary(".wav");
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::FileOutputStream> stream(temporary.getFile().createOutputStream());
        REQUIRE(stream != nullptr);
        std::unique_ptr<juce::AudioFormatWriter> writer(
                format.createWriterFor(stream.release(), rate, 1, 24, {}, 0));
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()));
    }
    const auto preview = loadReferenceWaveformHeatmap(temporary.getFile());
    const auto expected = referenceWaveformHeatmap(StereoBuffer(audio, 0, 5 * rate), 512, 96);
    REQUIRE(preview.isValid());
    REQUIRE(preview.getWidth() == 512);
    REQUIRE(preview.getHeight() == 96);
    for (int x = 0; x < 512; ++x) {
        for (int y = 0; y < 96; ++y) {
            REQUIRE(preview.getPixelAt(x, y) == expected.getPixelAt(x, y));
        }
    }
}

TEST_CASE("Reference waveform sums stereo and displays normalized audible detail",
        "[cycle-v2][reference][waveform]") {
    juce::AudioBuffer<float> stereo(2, 4);
    const float left[] { 0.001f, -0.002f, 0.003f, 0.f };
    const float right[] { 0.001f, -0.001f, -0.003f, 0.001f };
    stereo.copyFrom(0, 0, left, 4);
    stereo.copyFrom(1, 0, right, 4);
    const auto preview = referenceWaveformPreview(StereoBuffer(stereo));
    REQUIRE(preview.getNumChannels() == 1);
    REQUIRE(preview.getSample(0, 0) == Catch::Approx(2.f / 3.f));
    REQUIRE(preview.getSample(0, 1) == Catch::Approx(-1.f));
    REQUIRE(preview.getSample(0, 2) == Catch::Approx(0.f));
    REQUIRE(preview.getSample(0, 3) == Catch::Approx(1.f / 3.f));
    REQUIRE(stereo.getSample(0, 2) == left[2]);
    REQUIRE(stereo.getSample(1, 2) == right[2]);

    stereo.clear();
    const auto silent = referenceWaveformPreview(StereoBuffer(stereo));
    REQUIRE(silent.getMagnitude(0, 4) == 0.f);
}

TEST_CASE("Every sidebar and library action has a parseable SVG icon",
        "[cycle-v2][browser][icons]") {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::Image sheet(juce::Image::ARGB, (int) LibraryIcon::Count * 80, 128, true);
    juce::Graphics graphics(sheet);
    graphics.fillAll(juce::Colour(0xff18212a));
    for (int index = 0; index < (int) LibraryIcon::Count; ++index) {
        INFO(index);
        REQUIRE(LibraryIconRenderer::hasIcon((LibraryIcon) index));
        LibraryIconRenderer::paint(graphics, (LibraryIcon) index,
                { (float) index * 80 + 8, 8.f, 64.f, 64.f }, true);
        LibraryIconRenderer::paint(graphics, (LibraryIcon) index,
                { (float) index * 80 + 28, 88.f, 24.f, 24.f }, false);
    }
    const auto file = juce::File(CYCLE_V2_TEST_OUTPUT_DIR).getChildFile("library-icons.png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    const auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(juce::PNGImageFormat().writeImageToStream(sheet, *stream));
}
