#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "UI/ReferenceWaveformPreview.h"
#include "UI/LibraryIconRenderer.h"

using namespace CycleV2;

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
