#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "UI/ReferenceWaveformPreview.h"
#include "UI/LibraryIconRenderer.h"
#include <Algo/PeakCounter.h>
#include <UI/Panels/ScalarSurfaceMaterial.h>
#include <cmath>
#include <numeric>

using namespace CycleV2;

TEST_CASE("Reference waveform resolution switches at one visible zero crossing per pixel",
        "[cycle-v2][reference][waveform][resolution]") {
    std::vector<float> audio(6000);
    for (size_t index = 0; index < audio.size(); ++index) {
        audio[index] = index % 2 == 0 ? -0.5f : 0.5f;
    }
    const Buffer<float> samples(audio.data(), (int) audio.size());
    REQUIRE(PeakCounter::findZeroCrossings(samples).size() == 1001);
    const auto crossings = PeakCounter::findZeroCrossings(samples, 0);
    REQUIRE(crossings.size() == 5999);
    REQUIRE_FALSE(referenceWaveformResolution(crossings, 0, 6000, 1024).useLine);
    const auto exact = referenceWaveformResolution(crossings, 2000, 3024, 1024);
    REQUIRE(exact.crossings == 1024);
    REQUIRE(exact.useLine);
    REQUIRE_FALSE(referenceWaveformResolution(crossings, 2000, 3025, 1024).useLine);
    REQUIRE(referenceWaveformResolution(crossings, 2000, 3025, 2048).useLine);
    REQUIRE(referenceWaveformResolution({}, 0, 6000, 1024).useLine);
}

TEST_CASE("Waveform viewport framing retains recording amplitude and clips outside the vertical range",
        "[cycle-v2][reference][waveform][resolution]") {
    float constant[] { 0.25f, 0.25f, 0.25f };
    const Buffer<float> samples(constant, 3);
    const auto full = referenceWaveformLine(samples, 64, 64);
    REQUIRE(full.getPixelAt(30, 23).getAlpha() > 0);
    REQUIRE(full.getPixelAt(30, 0).getAlpha() == 0);
    const auto zoomed = referenceWaveformLine(samples, 64, 64, 0.25f, 0.25f);
    REQUIRE(zoomed.getPixelAt(30, 31).getAlpha() > 0);
    const auto outside = referenceWaveformDensity(samples, 8, 16, 0.6f, 0.2f);
    REQUIRE(std::all_of(outside.begin(), outside.end(), [](float value) { return value == 0.f; }));
}

TEST_CASE("Waveform lines retain fractional sample positions at deep zoom",
        "[cycle-v2][reference][waveform][resolution]") {
    float samples[] { -1.f, 1.f, -1.f };
    const auto image = referenceWaveformLine(Buffer<float>(samples, 3), 101, 101,
            0.f, 1.f, { 0.5f, 1.5f });
    REQUIRE(image.getPixelAt(0, 50).getAlpha() > 0);
    REQUIRE(image.getPixelAt(50, 0).getAlpha() > 0);
    REQUIRE(image.getPixelAt(100, 50).getAlpha() > 0);
}

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

TEST_CASE("Waveform palettes retain their full range and recolour fixed source statistics",
        "[cycle-v2][waveform][density][palette]") {
    std::vector<float> samples(2049);
    for (size_t index = 0; index < samples.size(); ++index) {
        samples[index] = (index % 2 == 0 ? -1.f : 1.f) * (index < 1024 ? 1.f : 0.1f);
    }
    const auto density = signalWaveformHeatDensity(Buffer<float>(samples.data(), (int) samples.size()));
    const auto original = density;
    for (const auto& entry : WaveformColourStyles::entries) {
        const auto gradient = WaveformColourStyles::gradient(entry.style);
        REQUIRE(gradient.isValid());
        int transitions {};
        for (int index = 1; index < gradient.getWidth(); ++index) {
            transitions += gradient.getPixelAt(index, 0) != gradient.getPixelAt(index - 1, 0) ? 1 : 0;
        }
        REQUIRE(transitions > 100);
        const auto heat = signalWaveformHeatField(density, entry.style);
        REQUIRE(heat.getWidth() == 768);
        REQUIRE(heat.getHeight() == 384);
        REQUIRE(density == original);
    }
    const auto magenta = WaveformColourStyles::gradient(WaveformColourStyles::Style::Magenta);
    REQUIRE(magenta.getPixelAt(0, 0) == juce::Colours::black);
    REQUIRE(magenta.getPixelAt(255, 0).getRed() > 240);
}

TEST_CASE("Waveform colour stays attached to the signal through resizing zoom and path rendering",
        "[cycle-v2][waveform][density][palette]") {
    std::vector<float> signal(32769);
    for (size_t index = 0; index < signal.size(); ++index) {
        signal[index] = (1.f - 0.7f * index / signal.size())
                * std::sin((float) index * juce::MathConstants<float>::twoPi / 31.f);
    }
    const Buffer<float> mono(signal.data(), (int) signal.size());
    const auto heat = signalWaveformHeatField(mono);
    const auto compact = renderSignalWaveform(mono, 385, 193, ZoomRect(), heat, false);
    const auto expanded = renderSignalWaveform(mono, 769, 385, ZoomRect(), heat, false);
    const auto blueHeat = signalWaveformHeatField(signalWaveformHeatDensity(mono), WaveformColourStyles::Style::Blues);
    const auto recoloured = recolourSignalWaveform(expanded, blueHeat, ZoomRect());
    int changedColours {};
    int changedCoverage {};
    for (int y = 0; y < expanded.getHeight(); ++y) {
        for (int x = 0; x < expanded.getWidth(); ++x) {
            const auto before = expanded.getPixelAt(x, y);
            const auto after = recoloured.getPixelAt(x, y);
            changedColours += before != after ? 1 : 0;
            changedCoverage += before.getAlpha() != after.getAlpha() ? 1 : 0;
        }
    }
    REQUIRE(changedColours > 1000);
    REQUIRE(changedCoverage == 0);
    const auto coverage = referenceWaveformDensity(mono, 769, 385);
    int lostCoverage {};
    int filledBackground {};
    for (int y = 0; y < 385; ++y) {
        for (int x = 0; x < 769; ++x) {
            const bool visited = coverage[(size_t) x * 385 + (size_t) y] > 0.f;
            const auto pixel = expanded.getPixelAt(x, y);
            lostCoverage += visited && pixel.getAlpha() == 0 ? 1 : 0;
            filledBackground += !visited && pixel.getAlpha() > 0 ? 1 : 0;
        }
    }
    REQUIRE(lostCoverage == 0);
    REQUIRE(filledBackground == 0);

    const auto visible = mono.section(8192, 16385);
    const ZoomRect viewport(0.25f, 0.25f, 0.5f, 0.5f);
    const auto zoomed = renderSignalWaveform(visible, 385, 193, viewport, heat, false);
    const auto path = renderSignalWaveform(visible, 385, 193, viewport, heat, true);
    int resizedMatches {};
    int zoomedMatches {};
    int pathMatches {};
    int mismatches {};
    for (int y = 0; y < 193; ++y) {
        for (int x = 0; x < 385; ++x) {
            const auto small = compact.getPixelAt(x, y);
            const auto large = expanded.getPixelAt(2 * x, 2 * y);
            if (small.getAlpha() > 0 && large.getAlpha() > 0) {
                ++resizedMatches;
                mismatches += small != large ? 1 : 0;
            }
            const auto original = expanded.getPixelAt(192 + x, 96 + y);
            const auto zoom = zoomed.getPixelAt(x, y);
            const auto line = path.getPixelAt(x, y);
            if (original.getAlpha() > 0 && zoom.getAlpha() > 0) {
                ++zoomedMatches;
                mismatches += original != zoom ? 1 : 0;
            }
            if (zoom.getAlpha() > 0 && line.getAlpha() == 255) {
                ++pathMatches;
                mismatches += zoom != line ? 1 : 0;
            }
        }
    }
    REQUIRE(resizedMatches > 1000);
    REQUIRE(zoomedMatches > 1000);
    REQUIRE(pathMatches > 1000);
    REQUIRE(mismatches == 0);
}

TEST_CASE("Program gradient strips match actual flat surface colours",
        "[cycle-v2][surface][gradient]") {
    constexpr int width = 129;
    std::vector<float> values(width * 2);
    for (int x = 0; x < width; ++x) {
        values[(size_t) x * 2] = values[(size_t) x * 2 + 1] = (float) x / (width - 1);
    }
    for (const auto style : { ScalarSurfaceTimeStyle::IcyHot13,
            ScalarSurfaceTimeStyle::IcyHotRaw, ScalarSurfaceTimeStyle::Bullion }) {
        const auto material = ScalarSurfaceMaterial::forTimeStyle(style);
        const auto gradient = ScalarSurfaceMaterialEvaluator::createGradientImage(material, width);
        const auto surface = ScalarSurfaceMaterialEvaluator::createImage(values.data(), (int) values.size(), width, 2, material);
        for (int x = 0; x < width; ++x) {
            REQUIRE(gradient.getPixelAt(x, 0) == surface.getPixelAt(x, 0));
        }
    }
}

TEST_CASE("Waveform bitmap fallback preserves coverage and source anchored colours",
        "[cycle-v2][waveform][palette][composition]") {
    constexpr int width = 128;
    constexpr int height = 64;
    juce::Image heat(juce::Image::ARGB, width, height, true);
    juce::Graphics heatGraphics(heat);
    heatGraphics.fillAll(juce::Colours::red);
    heatGraphics.setColour(juce::Colours::blue);
    heatGraphics.fillRect(width / 2, 0, width / 2, height);
    std::vector<float> samples(width * 8);
    for (size_t index = 0; index < samples.size(); ++index) {
        samples[index] = 0.7f * std::sin((float) index * 0.7f);
    }
    const auto coverage = signalWaveformCoverage(
            Buffer<float>(samples.data(), (int) samples.size()), width, height, ZoomRect(), false);
    const auto coloured = recolourSignalWaveform(coverage, heat, ZoomRect());
    const auto render = [&](const ZoomRect& viewport) {
        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Graphics graphics(image);
        paintWaveformImage(graphics, coloured, ZoomRect(), viewport, image.getBounds().toFloat());
        return image;
    };
    const auto full = render(ZoomRect());
    const auto zoomed = render(ZoomRect(0.5f, 0.f, 0.5f, 1.f));
    int occupied {};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            REQUIRE(full.getPixelAt(x, y).getAlpha() == coverage.getPixelAt(x, y).getAlpha());
            if (full.getPixelAt(x, y).getAlpha() > 0 && x > 2 && x < width - 2) {
                ++occupied;
                if (x < width / 2 - 2) {
                    REQUIRE(full.getPixelAt(x, y) == juce::Colours::red);
                } else if (x > width / 2 + 2) {
                    REQUIRE(full.getPixelAt(x, y) == juce::Colours::blue);
                }
            }
            if (zoomed.getPixelAt(x, y).getAlpha() == 255 && x > 4 && x < width - 4) {
                REQUIRE(zoomed.getPixelAt(x, y) == juce::Colours::blue);
            }
        }
    }
    REQUIRE(occupied > 1000);
}

TEST_CASE("Sparse waveform paths fill both polarities translucently and retain a thicker edge",
        "[cycle-v2][waveform][path][fill]") {
    for (const float amplitude : { -0.5f, 0.5f }) {
        float samples[] { amplitude, amplitude, amplitude };
        const Buffer<float> mono(samples, 3);
        const auto stroke = referenceWaveformLine(mono, 129, 129);
        const auto filled = referenceWaveformLine(mono, 129, 129, 0.f, 1.f, {}, { 3.f, 0.2f });
        const int edge = amplitude > 0.f ? 32 : 96;
        const int interior = amplitude > 0.f ? 48 : 80;
        const int outside = amplitude > 0.f ? 20 : 110;
        REQUIRE(stroke.getPixelAt(64, interior).getAlpha() == 0);
        REQUIRE(filled.getPixelAt(64, interior).getAlpha() > 0);
        REQUIRE(filled.getPixelAt(64, interior).getAlpha() < 80);
        REQUIRE(filled.getPixelAt(64, edge).getAlpha() == 255);
        REQUIRE(filled.getPixelAt(64, outside).getAlpha() == 0);
        const int widenedEdge = amplitude > 0.f ? edge - 2 : edge + 1;
        REQUIRE(stroke.getPixelAt(64, widenedEdge).getAlpha() == 0);
        REQUIRE(filled.getPixelAt(64, widenedEdge).getAlpha() > 0);
    }
    float samples[] { -0.5f, -0.5f };
    const auto cropped = referenceWaveformLine(Buffer<float>(samples, 2), 129, 129,
            0.6f, 0.2f, {}, { 3.f, 0.2f });
    REQUIRE(cropped.getPixelAt(64, 30).getAlpha() > 0);
    REQUIRE(cropped.getPixelAt(64, 110).getAlpha() == 0);
}

TEST_CASE("Filled waveform detail requires resolved oscillations at the same logical zoom on Retina",
        "[cycle-v2][waveform][resolution][fill]") {
    std::vector<int> crossings(200);
    std::iota(crossings.begin(), crossings.end(), 0);
    REQUIRE(referenceWaveformResolution(crossings, 0, 85, 1024).useFill);
    REQUIRE_FALSE(referenceWaveformResolution(crossings, 0, 86, 1024).useFill);
    REQUIRE(referenceWaveformResolution(crossings, 0, 85, 2048, 2.f).useFill);
    REQUIRE_FALSE(referenceWaveformResolution(crossings, 0, 86, 2048, 2.f).useFill);
    REQUIRE(referenceWaveformResolution(crossings, 0, 200, 1024).useLine);
    REQUIRE_FALSE(referenceWaveformResolution(crossings, 0, 200, 1024).useFill);
}
