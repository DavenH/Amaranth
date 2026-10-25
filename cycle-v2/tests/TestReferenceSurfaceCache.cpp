#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "UI/ReferenceSurfaceCache.h"
#include "UiIconData.h"

using namespace CycleV2;

TEST_CASE("Reference views preserve the folded grid and cache spectra by analysis revision",
        "[cycle-v2][reference][surface]") {
    const auto file = juce::File(CYCLE_V2_SOURCE_DIR).getParentDirectory()
            .getChildFile("cycle/content/wav/sine500.wav");
    auto analysis = ReferenceSampleAnalyzer::analyze(file);
    REQUIRE(analysis.succeeded());
    ReferenceSurfaceCache cache;
    REQUIRE(cache.refresh(analysis));
    const auto time = cache.surface(false);
    REQUIRE(time.values == analysis.cyclogram);
    REQUIRE(time.gridColumns == (size_t) analysis.columns);
    REQUIRE(time.gridRows == (size_t) analysis.rows);
    REQUIRE(cache.spectrumGeneration() == 0);
    const auto spectrum = cache.surface(true);
    REQUIRE(spectrum.connected);
    REQUIRE(spectrum.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(spectrum.gridColumns == time.gridColumns);
    REQUIRE(spectrum.values.size() == spectrum.gridColumns * spectrum.gridRows);
    REQUIRE(std::all_of(spectrum.values.begin(), spectrum.values.end(),
            [](float value) { return std::isfinite(value); }));
    REQUIRE(*std::max_element(spectrum.values.begin(), spectrum.values.end()) > 0.f);
    for (int toggle = 0; toggle < 4; ++toggle) {
        REQUIRE_FALSE(cache.refresh(analysis));
        REQUIRE(cache.surface(true).values == spectrum.values);
        REQUIRE(cache.surface(false).values == analysis.cyclogram);
    }
    REQUIRE(cache.generation() == 1);
    REQUIRE(cache.spectrumGeneration() == 1);
    ++analysis.sample->fundNote;
    ReferenceSampleAnalyzer::refreshCyclogram(analysis);
    REQUIRE(cache.refresh(analysis));
    REQUIRE(cache.surface(false).values != time.values);
    REQUIRE(cache.surface(true).values != spectrum.values);
    REQUIRE(cache.generation() == 2);
    REQUIRE(cache.spectrumGeneration() == 2);
}

TEST_CASE("Reference view and zoom icons render together at native size",
        "[cycle-v2][reference][icons]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const char* icons[] { UiIconData::referenceCyclogram, UiIconData::referenceSpectrogram,
            UiIconData::zoomAttack, UiIconData::envelopeZoomFit, UiIconData::zoomFull };
    juce::Image sheet(juce::Image::ARGB, 5 * 80, 128, true);
    juce::Graphics graphics(sheet);
    graphics.fillAll(juce::Colour(0xff10151d));
    for (int index = 0; index < 5; ++index) {
        const auto xml = juce::parseXML(icons[index]);
        REQUIRE(xml != nullptr);
        const auto icon = juce::Drawable::createFromSVG(*xml);
        REQUIRE(icon != nullptr);
        icon->drawWithin(graphics, { index * 80.f + 8, 8.f, 64.f, 64.f },
                juce::RectanglePlacement::centred, 1.f);
        icon->drawWithin(graphics, { index * 80.f + 33, 93.f, 14.f, 14.f },
                juce::RectanglePlacement::centred, 1.f);
    }
    const auto file = juce::File(CYCLE_V2_TEST_OUTPUT_DIR).getChildFile("reference-view-icons.png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    const auto stream = file.createOutputStream();
    REQUIRE(stream != nullptr);
    REQUIRE(juce::PNGImageFormat().writeImageToStream(sheet, *stream));
}
