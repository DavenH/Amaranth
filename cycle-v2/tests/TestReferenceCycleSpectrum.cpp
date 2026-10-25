#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <Algo/FFT.h>
#include <Audio/WavePeriodSpectrum.h>
#include <Util/NumberUtils.h>

#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"
#include "UI/ReferenceSurfaceCache.h"

using namespace CycleV2;

TEST_CASE("Cycle spectra start at harmonic one with Cycle 1 transform amplitudes",
        "[cycle-v2][reference][cycle-spectrum]") {
    for (double rate : { 44100.0, 48000.0, 96000.0 }) {
        const int note = 60 + 12;
        const int rows = WavePeriodSpectrum::cycleSize(note, rate);
        const int harmonics = WavePeriodSpectrum::harmonicCount(note, rate);
        REQUIRE((rows & (rows - 1)) == 0);
        REQUIRE(rows >= harmonics * 2);
        std::vector<float> cycle((size_t) rows);
        std::vector<float> output((size_t) harmonics);
        std::vector<float> phase((size_t) harmonics);
        for (int index = 0; index < rows; ++index) {
            const double phase = juce::MathConstants<double>::twoPi * index / rows;
            cycle[(size_t) index] = (float) (4.0 + 0.6 * std::sin(phase) + 0.12 * std::sin(7 * phase));
        }
        std::vector<Column> cycles { Column(cycle.data(), rows, 0.f, (char) note) };
        std::vector<Column> spectra { Column(output.data(), harmonics, 0.f, (char) note) };
        std::vector<Column> phases { Column(phase.data(), harmonics, 0.f, (char) note) };
        Transform transform;
        WavePeriodSpectrum::render(transform, cycles, spectra, &phases);
        REQUIRE(output[0] == Catch::Approx(0.3f).margin(1e-6f));
        REQUIRE(output[6] == Catch::Approx(0.06f).margin(1e-6f));
        REQUIRE(output.back() == 0.f);
        REQUIRE(phase[0] == Catch::Approx(-juce::MathConstants<float>::halfPi).margin(1e-6f));
        REQUIRE(phase[6] == Catch::Approx(-juce::MathConstants<float>::halfPi).margin(1e-6f));
        REQUIRE(phase.back() == 0.f);
        for (int harmonic = 0; harmonic < harmonics; ++harmonic) {
            if (harmonic != 0 && harmonic != 6) {
                REQUIRE(std::abs(output[(size_t) harmonic]) < 1e-6f);
            }
        }
        const auto profile = TrimeshRenderProfile::fromDomain(PortDomain::SpectralMagnitudeSignal);
        const auto display = profile.mapSpectrum2DGridToDisplay(output, 1, output.size(), note, false, rate);
        const double expected = std::log(1.000001 + 500 * juce::MathConstants<double>::twoPi * 0.3)
                / std::log(1.000001 + 500 * juce::MathConstants<double>::twoPi);
        REQUIRE(display.front() == Catch::Approx(expected).margin(1e-6));
        REQUIRE(display.front() > output.front());
    }
}

TEST_CASE("Reference FFTs stretch irregular tracked cycles independently of the thumbnail",
        "[cycle-v2][reference][cycle-spectrum]") {
    for (int rate : { 44100, 48000, 96000 }) {
        std::vector<float> audio;
        std::vector<PitchFrame> periods;
        for (int index = 0; index < 32; ++index) {
            const int lengths[] { 409, 463, 431, 487 };
            const int length = juce::roundToInt(lengths[index % 4] * rate / 44100.0);
            periods.emplace_back((int) audio.size(), (float) length, 0.f);
            for (int frame = 0; frame < length; ++frame) {
                const double phase = juce::MathConstants<double>::twoPi * frame / length;
                audio.push_back((float) (4 + 0.6 * std::sin(phase)
                        + 0.12 * std::sin(7 * phase) + 0.12 * std::sin(160 * phase)));
            }
        }
        ReferenceSampleAnalysis analysis;
        analysis.sample = std::make_unique<PitchedSample>(Buffer<float>(audio.data(), (int) audio.size()));
        analysis.sample->samplerate = rate;
        analysis.sample->fundNote = juce::roundToInt(NumberUtils::frequencyToNote(44100.f / 447.5f));
        analysis.sample->periods = periods;
        analysis.columns = 40;
        analysis.rows = 256;
        // A display-only thumbnail must never become the spectral analysis source.
        analysis.cyclogram.resize((size_t) analysis.columns * (size_t) analysis.rows, 0.f);
        analysis.cyclogramRevision = 1;
        ReferenceSurfaceCache cache;
        REQUIRE(cache.refresh(analysis));
        const auto& spectrum = cache.surface(true);
        REQUIRE(spectrum.gridRows > 160);
        double fundamental = 0;
        double seventh = 0;
        double upper = 0;
        for (size_t column = 5; column < 30; ++column) {
            const size_t offset = column * spectrum.gridRows;
            fundamental += spectrum.values[offset];
            seventh += spectrum.values[offset + 6];
            upper += spectrum.values[offset + 159];
        }
        REQUIRE(fundamental / 25 > 0.1);
        REQUIRE(seventh / fundamental == Catch::Approx(0.2).margin(0.03));
        REQUIRE(upper / 25 > 0.001);
        REQUIRE(cache.spectrumGeneration() == 1);
        REQUIRE_FALSE(cache.refresh(analysis));
        REQUIRE(cache.surface(true).values == spectrum.values);
        REQUIRE(cache.spectrumGeneration() == 1);
    }
}

TEST_CASE("Reference frequency display caps rows after full harmonic analysis",
        "[cycle-v2][reference][resolution]") {
    constexpr int rows = 4096;
    std::vector<float> magnitudes(rows, 0.2f);
    magnitudes.front() = 0.6f;
    const auto profile = TrimeshRenderProfile::fromDomain(PortDomain::SpectralMagnitudeSignal);
    const auto full = profile.mapSpectrum2DGridToDisplay(magnitudes, 1, rows, 36, false, 96000.0);
    const auto capped = profile.mapSpectrum2DGridToDisplay(magnitudes, 1, rows, 36, false, 96000.0, 1024);
    REQUIRE(capped.size() == 1024);
    REQUIRE(capped.front() == full.front());
    REQUIRE(capped.back() == full.back());
    REQUIRE(*std::min_element(capped.begin(), capped.end()) > 0.6f);
}
