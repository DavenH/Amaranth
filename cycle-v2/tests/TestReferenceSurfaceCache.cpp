#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <Audio/WavePeriodFolding.h>
#include <Util/NumberUtils.h>

#include "UI/ReferenceSurfaceCache.h"
#include "UiIconData.h"

using namespace CycleV2;

TEST_CASE("Reference detail selects cycles evenly and retains their recording times",
        "[cycle-v2][reference][resolution]") {
    for (int count : { 32, 1024, 1536, 2048 }) {
        INFO(count);
        std::vector<float> audio;
        std::vector<PitchFrame> frames;
        for (int index = 0; index < count; ++index) {
            const int length = index % 2 == 0 ? 53 : 71;
            frames.emplace_back((int) audio.size(), (float) length, 0.f);
            for (int sample = 0; sample < length; ++sample) {
                audio.push_back((float) std::sin(juce::MathConstants<double>::twoPi * sample / length)
                        * (float) (index + 1) / (float) count);
            }
        }
        ReferenceSampleAnalysis analysis;
        analysis.sample = std::make_unique<PitchedSample>(Buffer<float>(audio.data(), (int) audio.size()));
        analysis.sample->fundNote = 72;
        analysis.sample->periods = frames;
        ReferenceSampleAnalyzer::refreshCyclogram(analysis);
        REQUIRE(analysis.columns == std::min(count, 1024));
        REQUIRE(analysis.rows == 128);
        REQUIRE(analysis.cyclogram.size() == (size_t) analysis.columns * 128);

        std::vector<float> dense((size_t) count * 128);
        std::vector<Column> columns;
        for (const auto& frame : frames) {
            columns.emplace_back(dense.data() + columns.size() * 128, 128,
                    (float) frame.sampleOffset / (float) audio.size(), 72);
        }
        WavePeriodFolding::render(*analysis.sample, columns, analysis.sample->size(), true, true, 0.5, false, true);
        for (int index = 0; index < analysis.columns; ++index) {
            const size_t selected = (size_t) index * (size_t) count / (size_t) analysis.columns;
            REQUIRE(analysis.cyclePositions[(size_t) index]
                    == (float) frames[selected].sampleOffset / (float) audio.size());
            for (int row = 0; row < 128; ++row) {
                REQUIRE(analysis.cyclogram[(size_t) index * 128 + (size_t) row]
                        == Catch::Approx(dense[selected * 128 + (size_t) row]).margin(1e-6));
            }
        }
        const auto last = analysis.cyclogram.end() - analysis.rows;
        REQUIRE(std::any_of(last, analysis.cyclogram.end(), [](float value) { return std::abs(value) > 0.1f; }));
    }
}

TEST_CASE("Long reference cycles are resampled into at most 1024 phase rows",
        "[cycle-v2][reference][resolution]") {
    std::vector<float> audio(18000);
    Buffer<float>(audio.data(), (int) audio.size()).ramp(-1.f, 2.f / (float) audio.size());
    ReferenceSampleAnalysis analysis;
    analysis.sample = std::make_unique<PitchedSample>(Buffer<float>(audio.data(), (int) audio.size()));
    analysis.sample->fundNote = 40;
    analysis.sample->periods = { { 0, 2000.f, 0.f }, { 2000, 2300.f, 0.f },
            { 4300, 2100.f, 0.f }, { 6400, 2000.f, 0.f } };
    ReferenceSampleAnalyzer::refreshCyclogram(analysis);
    REQUIRE(analysis.columns == 4);
    REQUIRE(analysis.rows == 1024);
    REQUIRE(std::all_of(analysis.cyclogram.begin(), analysis.cyclogram.end(),
            [](float value) { return std::isfinite(value); }));
    REQUIRE(*std::min_element(analysis.cyclogram.begin(), analysis.cyclogram.end()) < -0.1f);
}

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
