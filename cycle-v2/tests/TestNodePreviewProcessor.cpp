#include <algorithm>
#include <cmath>
#include <numeric>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Runtime/NodePreviewProcessor.h"
#include "Nodes/Delay/DelayPreviewPainter.h"
#include "Nodes/Equalizer/EqualizerPreviewPainter.h"
#include "Nodes/Reverb/ReverbPreviewPainter.h"
#include "Nodes/Unison/UnisonPreviewPainter.h"
#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"
#include "UI/NodePreviewRenderer.h"

#include <Util/Arithmetic.h>
#include <Util/LogRegionMapping.h>

using namespace CycleV2;

namespace {

bool hasColouredPixel(const Image& image) {
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            const Colour colour = image.getPixelAt(x, y);
            if (colour.getRed() != colour.getGreen()
                    || colour.getGreen() != colour.getBlue()) {
                return true;
            }
        }
    }
    return false;
}

Colour storedArgbPixel(Colour colour) {
    Image image(Image::ARGB, 1, 1, true);
    image.setPixelAt(0, 0, colour);
    return image.getPixelAt(0, 0);
}

bool imagesMatch(const Image& first, const Image& second) {
    if (first.getBounds() != second.getBounds()) {
        return false;
    }

    for (int y = 0; y < first.getHeight(); ++y) {
        for (int x = 0; x < first.getWidth(); ++x) {
            if (first.getPixelAt(x, y) != second.getPixelAt(x, y)) {
                return false;
            }
        }
    }

    return true;
}

std::vector<float> localizedSpectralRegion(
        size_t rows,
        float start,
        float end,
        int midiNote) {
    std::vector<float> positions(rows);
    LogRegionMapping(midiNote).fillDisplayUnits(Buffer<float>(
            positions.data(),
            (int) positions.size()));
    std::vector<float> values(rows);
    for (size_t row = 0; row < rows; ++row) {
        values[row] = positions[row] >= start && positions[row] <= end
                ? 1.f
                : 0.f;
    }
    return values;
}

std::pair<float, float> activeRegion(
        const std::vector<float>& values,
        float threshold) {
    const auto first = std::find_if(values.begin(), values.end(), [&](float value) {
        return value > threshold;
    });
    const auto last = std::find_if(values.rbegin(), values.rend(), [&](float value) {
        return value > threshold;
    });
    if (first == values.end() || last == values.rend()) {
        return {};
    }

    const float denominator = (float) (values.size() - 1);
    return {
            (float) std::distance(values.begin(), first) / denominator,
            (float) (values.size() - 1 - (size_t) std::distance(values.rbegin(), last))
                    / denominator
    };
}

}

TEST_CASE("Node preview processor factory creates preview modules", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;

    const PreviewModuleRole roles[] {
            PreviewModuleRole::VoiceContext,
            PreviewModuleRole::Waveform,
            PreviewModuleRole::Image,
            PreviewModuleRole::MeshSurface,
            PreviewModuleRole::SpectrumMagnitude,
            PreviewModuleRole::SpectrumPhase,
            PreviewModuleRole::Envelope,
            PreviewModuleRole::ImpulseResponse,
            PreviewModuleRole::Waveshaper,
            PreviewModuleRole::ReverbSpectrogram,
            PreviewModuleRole::EqualizerResponse,
            PreviewModuleRole::SignalSpy,
            PreviewModuleRole::OutputMeters,
            PreviewModuleRole::Generic
    };

    for (const PreviewModuleRole role : roles) {
        CAPTURE(role);
        auto processor = factory.create(role);
        REQUIRE(processor != nullptr);
        REQUIRE(processor->role() == role);
    }

    REQUIRE(factory.create(PreviewModuleRole::None) == nullptr);
}

TEST_CASE("Runtime preview fingerprints include both visible channels",
        "[cycle-v2][runtime][ui][cache]") {
    NodePreviewResult preview {
            "output",
            PreviewModuleRole::OutputMeters,
            { 0.4f },
            { 0.6f }
    };
    const uint64_t original = nodePreviewResultFingerprint(preview);

    preview.secondary.front() = 0.8f;
    REQUIRE(nodePreviewResultFingerprint(preview) != original);
    preview.secondary.front() = 0.6f;
    preview.primary.front() = 0.2f;
    REQUIRE(nodePreviewResultFingerprint(preview) != original);
}

TEST_CASE("Signal spy heatmaps reveal low-amplitude time signals",
        "[cycle-v2][runtime][probe][ui]") {
    NodePreviewResult result;
    result.role = PreviewModuleRole::SignalSpy;
    result.primary = { -0.01f, 0.01f, -0.005f, 0.005f };
    result.gridColumns = 2;
    result.gridRows = 2;
    result.domain = PortDomain::TimeSignal;

    const Image image = NodePreviewRenderer::createRuntimeHeatmapImage(result);

    REQUIRE(image.isValid());
    CHECK(image.getPixelAt(0, 1) != image.getPixelAt(0, 0));
    CHECK(image.getPixelAt(1, 1) != image.getPixelAt(1, 0));
}

TEST_CASE("Signal spy heatmaps preserve absolute time-signal gain",
        "[cycle-v2][runtime][probe][ui]") {
    const auto render = [](float gain) {
        NodePreviewResult result;
        result.role = PreviewModuleRole::SignalSpy;
        result.primary = { -gain, gain, -gain * 0.5f, gain * 0.5f };
        result.gridColumns = 2;
        result.gridRows = 2;
        result.domain = PortDomain::TimeSignal;
        return NodePreviewRenderer::createRuntimeHeatmapImage(result);
    };

    const Image quiet = render(0.25f);
    const Image loud = render(0.5f);
    REQUIRE(quiet.isValid());
    REQUIRE(loud.isValid());

    bool differs {};
    for (int y = 0; y < quiet.getHeight(); ++y) {
        for (int x = 0; x < quiet.getWidth(); ++x) {
            differs = differs || quiet.getPixelAt(x, y) != loud.getPixelAt(x, y);
        }
    }
    REQUIRE(differs);
}

TEST_CASE("Spectral preview frequency mapping follows the Cycle logarithmic sampler",
        "[cycle-v2][runtime][probe][spectral][ui]") {
    constexpr size_t rows = 257;
    constexpr float regionStart = 0.24f;
    constexpr float regionEnd = 0.34f;
    std::vector<float> source(rows);
    const size_t sourceStart = 1 + (size_t) (regionStart * (float) (rows - 2));
    const size_t sourceEnd = 1 + (size_t) (regionEnd * (float) (rows - 2));
    std::fill(
            source.begin() + (std::vector<float>::difference_type) sourceStart,
            source.begin() + (std::vector<float>::difference_type) sourceEnd,
            1.f);

    const auto profile = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal);
    const auto c3 = profile.mapGridToDisplay(source, 1, rows, 48);
    const auto c5 = profile.mapGridToDisplay(source, 1, rows, 72);
    const auto firstActiveRow = [](const std::vector<float>& values) {
        return (size_t) std::distance(
                values.begin(),
                std::find_if(values.begin(), values.end(), [](float value) {
                    return value > 0.5f;
                }));
    };

    const size_t c3Start = firstActiveRow(c3);
    const size_t c5Start = firstActiveRow(c5);
    const size_t expectedC3 = (size_t) roundToInt(
            profile.displayFrequencyUnit(regionStart, 48) * (float) (rows - 1));
    const size_t expectedC5 = (size_t) roundToInt(
            profile.displayFrequencyUnit(regionStart, 72) * (float) (rows - 1));

    CHECK(std::abs((int) c3Start - (int) expectedC3) <= 2);
    CHECK(std::abs((int) c5Start - (int) expectedC5) <= 2);
    CHECK(c3Start > c5Start);
}

TEST_CASE("Spectral preview magnitude mapping follows Spectrum2D",
        "[cycle-v2][runtime][probe][spectral][ui]") {
    constexpr size_t rows = 5;
    const std::vector<float> source { 1000.f, 0.f, 0.001f, 0.1f, 1.f };
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal);
    const auto c3 = profile.mapSpectrum2DGridToDisplay(source, 1, rows, 48);
    const auto c5 = profile.mapSpectrum2DGridToDisplay(source, 1, rows, 72);
    const float expectedC3[] { 0.000000118f, 0.007274421f, 0.036858425f, 0.14368251f, 1.f };
    const float expectedC5[] { 0.000000118f, 0.017031401f, 0.067444496f, 0.238788915f, 1.f };

    for (size_t row = 0; row < rows; ++row) {
        CHECK(c3[row] == Catch::Approx(expectedC3[row]).margin(1.0e-6f));
        CHECK(c5[row] == Catch::Approx(expectedC5[row]).margin(1.0e-6f));
    }
}

TEST_CASE("Spectral compact and expanded grids preserve pitch-mapped value regions",
        "[cycle-v2][runtime][probe][spectral][ui][integration]") {
    constexpr float sourceStart = 0.35f;
    constexpr float sourceEnd = 0.70f;
    constexpr size_t compactRows = 65;
    constexpr size_t expandedRows = 513;

    for (const PortDomain domain : {
            PortDomain::SpectralMagnitudeSignal,
            PortDomain::SpectralPhaseSignal }) {
        const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(domain);
        float previousSampledStart {};

        for (const int midiNote : { 72, 48 }) {
            const auto compactSamples = localizedSpectralRegion(
                    compactRows,
                    sourceStart,
                    sourceEnd,
                    midiNote);
            const auto expandedSamples = localizedSpectralRegion(
                    expandedRows,
                    sourceStart,
                    sourceEnd,
                    midiNote);
            const auto compact = profile.mapGridToDisplay(
                    compactSamples,
                    1,
                    compactRows,
                    midiNote);
            const auto expanded = profile.mapGridToDisplay(
                    expandedSamples,
                    1,
                    expandedRows,
                    midiNote);
            const float threshold = domain == PortDomain::SpectralMagnitudeSignal
                    ? 0.5f
                    : 0.75f;
            const auto compactRegion = activeRegion(compact, threshold);
            const auto expandedRegion = activeRegion(expanded, threshold);
            const auto sampledRegion = activeRegion(expandedSamples, 0.5f);

            CHECK(compactRegion.first == Catch::Approx(expandedRegion.first).margin(0.04f));
            CHECK(compactRegion.second == Catch::Approx(expandedRegion.second).margin(0.04f));
            CHECK(expandedRegion.first == Catch::Approx(sourceStart).margin(0.02f));
            CHECK(expandedRegion.second == Catch::Approx(sourceEnd).margin(0.02f));
            if (previousSampledStart > 0.f) {
                CHECK(std::abs(sampledRegion.first - previousSampledStart) > 0.005f);
            }
            previousSampledStart = sampledRegion.first;
        }
    }
}

TEST_CASE("Trimesh spectral presentation preserves the authored value scale",
        "[cycle-v2][runtime][preview][spectral][ui][integration]") {
    const TrimeshRenderProfile magnitude = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal);
    const TrimeshRenderProfile multiplicative = TrimeshRenderProfile::fromSemantic({
            PortDomain::SpectralMagnitudeSignal,
            RenderScalePolicy::Bipolar,
            RenderSemanticRole::SpectralMagnitudeMultiplicative
    });
    const TrimeshRenderProfile phase = TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralPhaseSignal);

    const std::vector<float> magnitudeValues { 0.f, 0.5f, 0.75f, 1.f };
    const std::vector<float> phaseValues { -1.f, 0.f, 1.f };
    const std::vector<float> expectedPhase { 0.f, 0.5f, 1.f };

    CHECK(magnitude.mapTrimeshValuesToDisplay(magnitudeValues) == magnitudeValues);
    CHECK(multiplicative.mapTrimeshValuesToDisplay(magnitudeValues) == magnitudeValues);
    CHECK(phase.mapTrimeshValuesToDisplay(phaseValues) == expectedPhase);
    const std::vector<float> halfMagnitude { 0.5f, 0.5f };
    CHECK(magnitude.mapSpectrum2DGridToDisplay(halfMagnitude, 1, 2, 48)
            != halfMagnitude);
}

TEST_CASE("Magnitude mesh heatmaps consume the full unipolar colour scale",
        "[cycle-v2][runtime][preview][spectral][ui]") {
    NodePreviewResult result;
    result.role = PreviewModuleRole::MeshSurface;
    result.primary = { 0.f, 0.f, 0.25f, 0.25f, 0.5f, 0.5f,
            0.75f, 0.75f, 1.f, 1.f };
    result.gridColumns = 5;
    result.gridRows = 2;
    result.domain = PortDomain::SpectralMagnitudeSignal;

    const Image image = NodePreviewRenderer::createRuntimeHeatmapImage(result);
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(result.domain);
    const auto expected = profile.mapGridToDisplay(
            result.primary,
            result.gridColumns,
            result.gridRows);

    REQUIRE(image.isValid());
    for (int column = 0; column < image.getWidth(); ++column) {
        CAPTURE(column);
        const Colour actual = image.getPixelAt(column, 0);
        if (column == 0) {
            CHECK(actual.getAlpha() == 0);
        } else {
            CHECK(actual == profile.getSurfaceStyle().colourForValue(
                    expected[(size_t) column * result.gridRows]));
        }
    }
}

TEST_CASE("Spectral grid mapping is identical for Trimesh and its signal spies",
        "[cycle-v2][runtime][preview][probe][spectral][ui]") {
    for (const PortDomain domain : {
            PortDomain::SpectralMagnitudeSignal,
            PortDomain::SpectralPhaseSignal }) {
        NodePreviewResult result;
        result.role = PreviewModuleRole::MeshSurface;
        result.primary = {
                0.f, 0.1f, 0.8f, 0.2f,
                0.f, 0.2f, 0.4f, 0.9f,
                0.f, 0.7f, 0.3f, 0.1f
        };
        if (domain == PortDomain::SpectralPhaseSignal) {
            Buffer<float>(result.primary.data(), (int) result.primary.size())
                    .mul(2.f)
                    .add(-1.f);
        }
        result.gridColumns = 3;
        result.gridRows = 4;
        result.domain = domain;
        result.frequencySampling = TraversalGridFrequencySampling::LinearBins;
        result.frequencyMidiNote = 48;

        const Image trimesh = NodePreviewRenderer::createRuntimeHeatmapImage(result);
        const Image spy = NodePreviewRenderer::createRuntimeHeatmapImage(result);

        CAPTURE(domain);
        REQUIRE(trimesh.isValid());
        REQUIRE(spy.isValid());
        REQUIRE(trimesh.getBounds() == spy.getBounds());
        for (int y = 0; y < trimesh.getHeight(); ++y) {
            for (int x = 0; x < trimesh.getWidth(); ++x) {
                REQUIRE(trimesh.getPixelAt(x, y) == spy.getPixelAt(x, y));
            }
        }
    }
}

TEST_CASE("Phase mesh heatmaps convert bipolar values exactly once",
        "[cycle-v2][runtime][preview][spectral][ui]") {
    NodePreviewResult result;
    result.role = PreviewModuleRole::MeshSurface;
    result.primary = { -1.f, -1.f, 0.f, 0.f, 1.f, 1.f };
    result.gridColumns = 3;
    result.gridRows = 2;
    result.domain = PortDomain::SpectralPhaseSignal;

    const Image image = NodePreviewRenderer::createRuntimeHeatmapImage(result);
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(result.domain);
    const float expected[] { 0.f, 0.5f, 1.f };

    REQUIRE(image.isValid());
    for (int column = 0; column < image.getWidth(); ++column) {
        CAPTURE(column);
        CHECK(image.getPixelAt(column, 0) == storedArgbPixel(
                profile.getSurfaceStyle().colourForValue(expected[column])));
    }
}

TEST_CASE("Spectral spy heatmaps map the raw Trimesh grid exactly once",
        "[cycle-v2][runtime][probe][spectral][ui]") {
    NodePreviewResult mesh;
    mesh.role = PreviewModuleRole::MeshSurface;
    mesh.primary = {
            0.f, 0.f, 0.1f, 0.35f, 0.8f, 1.f, 0.75f, 0.2f, 0.f,
            0.f, 0.05f, 0.25f, 0.65f, 1.f, 0.9f, 0.4f, 0.1f, 0.f
    };
    mesh.gridColumns = 2;
    mesh.gridRows = 9;
    mesh.domain = PortDomain::SpectralMagnitudeSignal;
    mesh.frequencySampling = TraversalGridFrequencySampling::LinearBins;
    mesh.frequencyMidiNote = 48;

    NodePreviewResult spy = mesh;
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromSemantic({
            PortDomain::SpectralMagnitudeSignal,
            RenderScalePolicy::Bipolar,
            RenderSemanticRole::SpectralMagnitudeMultiplicative
    });

    const Image meshImage = NodePreviewRenderer::createRuntimeHeatmapImage(mesh, profile);
    const Image spyImage = NodePreviewRenderer::createRuntimeHeatmapImage(spy, profile);
    TrimeshRenderData expectedData;
    expectedData.surface = mesh.primary;
    expectedData.columns = (int) mesh.gridColumns;
    expectedData.rows = (int) mesh.gridRows;
    expectedData.domain = mesh.domain;
    expectedData.surface = profile.mapGridToDisplay(
            expectedData.surface,
            mesh.gridColumns,
            mesh.gridRows,
            mesh.frequencyMidiNote);
    const Image expectedImage = TrimeshSurfaceRenderer::createHeatmapImage(
            expectedData,
            profile);

    REQUIRE(meshImage.isValid());
    REQUIRE(spyImage.isValid());
    REQUIRE(expectedImage.isValid());
    REQUIRE(imagesMatch(meshImage, expectedImage));
    REQUIRE(imagesMatch(spyImage, expectedImage));

    mesh.primary.assign(mesh.primary.size(), 0.f);
    const Image zeroImage = NodePreviewRenderer::createRuntimeHeatmapImage(mesh, profile);
    expectedData.surface = profile.mapGridToDisplay(
            mesh.primary,
            mesh.gridColumns,
            mesh.gridRows,
            mesh.frequencyMidiNote);
    const Image expectedZeroImage = TrimeshSurfaceRenderer::createHeatmapImage(
            expectedData,
            profile);
    REQUIRE(zeroImage.isValid());
    REQUIRE(expectedZeroImage.isValid());
    CHECK(imagesMatch(zeroImage, expectedZeroImage));
}

TEST_CASE("FFT magnitude spy heatmaps map linear bins and amplitude once",
        "[cycle-v2][runtime][probe][spectral][ui]") {
    NodePreviewResult spy;
    spy.role = PreviewModuleRole::SignalSpy;
    spy.primary = {
            1000.f, 0.f, 0.001f, 0.01f, 0.1f, 0.5f, 1.f, 0.5f, 0.1f,
            1000.f, 0.f, 0.001f, 0.01f, 0.1f, 0.5f, 1.f, 0.5f, 0.1f
    };
    spy.gridColumns = 2;
    spy.gridRows = spy.primary.size() / spy.gridColumns;
    spy.domain = PortDomain::SpectralMagnitudeSignal;
    spy.frequencySampling = TraversalGridFrequencySampling::LinearBins;

    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(spy.domain);
    TrimeshRenderData expected;
    expected.surface = profile.mapSpectrum2DGridToDisplay(
            spy.primary,
            spy.gridColumns,
            spy.gridRows);
    expected.columns = (int) spy.gridColumns;
    expected.rows = (int) spy.gridRows;
    expected.domain = spy.domain;

    const Image spyImage = NodePreviewRenderer::createRuntimeHeatmapImage(spy, profile);
    const Image expectedImage = TrimeshSurfaceRenderer::createHeatmapImage(
            expected,
            profile);

    REQUIRE(spyImage.isValid());
    REQUIRE(expectedImage.isValid());
    REQUIRE(imagesMatch(spyImage, expectedImage));
}

TEST_CASE("Disabled compact effect previews are greyscale",
        "[cycle-v2][effects][preview]") {
    for (const NodeKind kind : {
            NodeKind::Unison,
            NodeKind::Delay,
            NodeKind::Reverb,
            NodeKind::Equalizer }) {
        Node enabled;
        enabled.kind = kind;
        enabled.parameters.push_back({ "enabled", "Enabled", "1" });
        Node disabled = enabled;
        disabled.parameters.front().value = "0";

        Image enabledImage(Image::RGB, 200, 60, true);
        Graphics enabledGraphics(enabledImage);
        const auto paint = [](Graphics& graphics, Rectangle<float> area, const Node& node) {
            if (node.kind == NodeKind::Unison) {
                UnisonPreviewPainter().paint(graphics, area, node, 1.f);
            } else if (node.kind == NodeKind::Delay) {
                DelayPreviewPainter().paint(graphics, area, node, 1.f);
            } else if (node.kind == NodeKind::Reverb) {
                ReverbPreviewPainter().paint(graphics, area, node, 1.f);
            } else {
                EqualizerPreviewPainter().paint(graphics, area, node, false);
            }
        };
        paint(enabledGraphics, enabledImage.getBounds().toFloat(), enabled);

        Image disabledImage(Image::RGB, 200, 60, true);
        Graphics disabledGraphics(disabledImage);
        paint(disabledGraphics, disabledImage.getBounds().toFloat(), disabled);

        REQUIRE(hasColouredPixel(enabledImage));
        REQUIRE_FALSE(hasColouredPixel(disabledImage));
    }
}

TEST_CASE("Disabled Equalizer response retains a greyscale configured curve",
        "[cycle-v2][effects][preview][equalizer]") {
    ScopedJuceInitialiser_GUI juce;
    Node enabled;
    enabled.kind = NodeKind::Equalizer;
    enabled.parameters = {
            { "enabled", "Enabled", "1" },
            { "band1Gain", "Band 1 Gain", "0.7" },
            { "band1Frequency", "Band 1 Frequency", "0.2" },
            { "band3Gain", "Band 3 Gain", "0.3" },
            { "band3Frequency", "Band 3 Frequency", "0.55" }
    };
    Node disabled = enabled;
    disabled.parameters.front().value = "0";

    Image enabledImage(Image::RGB, 500, 120, true);
    Graphics enabledGraphics(enabledImage);
    EqualizerPreviewPainter().paint(
            enabledGraphics,
            enabledImage.getBounds().toFloat(),
            enabled,
            true);

    Image disabledImage(Image::RGB, 500, 120, true);
    Graphics disabledGraphics(disabledImage);
    EqualizerPreviewPainter().paint(
            disabledGraphics,
            disabledImage.getBounds().toFloat(),
            disabled,
            true);

    REQUIRE(hasColouredPixel(enabledImage));
    REQUIRE_FALSE(hasColouredPixel(disabledImage));
}

TEST_CASE("Reverb preview spectrogram analyzes the generated kernel",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    NodePreviewProcessorFactory factory;
    auto processor = factory.create(PreviewModuleRole::ReverbSpectrogram);
    REQUIRE(processor != nullptr);

    const std::vector<NodeParameter> parameters {
            { "enabled", "Enabled", "1" },
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "width", "Width", "1" },
            { "wet", "Wet", "0.8" },
            { "highPass", "High Pass", "0.05" }
    };
    const auto configuration = ReverbSignalProcessor::buildConfiguration(parameters);
    const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
    PreviewProcessContext context;
    context.pointCount = 24;
    context.parameters = parameters;
    context.configuration = &published;

    processor->render(context);

    REQUIRE(context.gridColumns == 256);
    REQUIRE(context.gridRows == 1025);
    REQUIRE(context.primary.size() == context.gridColumns * context.gridRows);
    REQUIRE(context.secondary.size() == context.primary.size());
    REQUIRE(context.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(*std::max_element(context.primary.begin(), context.primary.end()) > 0.f);
}

TEST_CASE("Reverb spectrogram preserves wet high-pass and size semantics",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    auto render = [](float size, float damp, float highPass, float wet) {
        const std::vector<NodeParameter> parameters {
                { "enabled", "Enabled", "1" },
                { "size", "Size", String(size) },
                { "damp", "Damp", String(damp) },
                { "width", "Width", "1" },
                { "wet", "Wet", String(wet) },
                { "highPass", "High Pass", String(highPass) }
        };
        const auto configuration = ReverbSignalProcessor::buildConfiguration(parameters);
        const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
        PreviewProcessContext context;
        context.pointCount = 24;
        context.parameters = parameters;
        context.configuration = &published;
        auto processor = NodePreviewProcessorFactory().create(
                PreviewModuleRole::ReverbSpectrogram);
        processor->render(context);
        return context;
    };

    const auto dry = render(0.f, 1.f, 0.f, 0.f);
    const auto lowWet = render(0.f, 1.f, 0.f, 0.2f);
    const auto highWet = render(0.f, 1.f, 0.f, 1.f);
    REQUIRE(std::accumulate(dry.primary.begin(), dry.primary.end(), 0.f) == 0.f);
    REQUIRE(std::accumulate(highWet.primary.begin(), highWet.primary.end(), 0.f)
            > std::accumulate(lowWet.primary.begin(), lowWet.primary.end(), 0.f));

    const auto highPassed = render(0.f, 1.f, 1.f, 1.f);
    auto spectralCentroid = [](const PreviewProcessContext& context) {
        double energy {};
        double weightedEnergy {};
        for (size_t column = 0; column < context.gridColumns; ++column) {
            const auto first = context.primary.begin()
                    + (std::vector<float>::difference_type) (column * context.gridRows);
            for (size_t row = 0; row < context.gridRows; ++row) {
                energy += first[(std::vector<float>::difference_type) row];
                weightedEnergy += (double) row
                        * first[(std::vector<float>::difference_type) row];
            }
        }
        return weightedEnergy / energy;
    };
    REQUIRE(spectralCentroid(highPassed) > spectralCentroid(highWet));

    const auto defaultUnfiltered = render(0.5f, 0.2f, 0.f, 1.f);
    const auto defaultHighPassed = render(0.5f, 0.2f, 1.f, 1.f);
    REQUIRE(std::accumulate(
                    defaultHighPassed.primary.begin(),
                    defaultHighPassed.primary.end(),
                    0.f)
            < 0.98f * std::accumulate(
                    defaultUnfiltered.primary.begin(),
                    defaultUnfiltered.primary.end(),
                    0.f));

    const auto largeRoom = render(1.f, 1.f, 0.f, 1.f);
    REQUIRE(largeRoom.gridColumns > highWet.gridColumns);
}

TEST_CASE("Bypassed Reverb spectrogram retains its configured response",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    const std::vector<NodeParameter> parameters {
            { "enabled", "Enabled", "0" },
            { "size", "Size", "0.5" },
            { "damp", "Damp", "0.2" },
            { "width", "Width", "1" },
            { "wet", "Wet", "0.4" },
            { "highPass", "High Pass", "0.05" }
    };
    const auto configuration = ReverbSignalProcessor::buildConfiguration(parameters);
    const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
    PreviewProcessContext context;
    context.pointCount = 24;
    context.parameters = parameters;
    context.configuration = &published;
    auto processor = NodePreviewProcessorFactory().create(
            PreviewModuleRole::ReverbSpectrogram);

    processor->render(context);

    REQUIRE(std::accumulate(context.primary.begin(), context.primary.end(), 0.f) > 0.f);

    NodePreviewResult result;
    result.role = PreviewModuleRole::ReverbSpectrogram;
    result.primary = std::move(context.primary);
    result.secondary = std::move(context.secondary);
    result.gridColumns = context.gridColumns;
    result.gridRows = context.gridRows;
    result.domain = context.domain;
    const Image image = NodePreviewRenderer::createRuntimeHeatmapImage(result, true);

    float maximumBrightness {};
    bool greyscale = true;
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            const Colour colour = image.getPixelAt(x, y);
            maximumBrightness = jmax(maximumBrightness, colour.getPerceivedBrightness());
            greyscale = greyscale
                    && colour.getRed() == colour.getGreen()
                    && colour.getGreen() == colour.getBlue();
        }
    }
    REQUIRE(greyscale);
    REQUIRE(maximumBrightness > 0.25f);
}

TEST_CASE("Reverb Width preview follows production stereo mixing",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    const auto render = [](float width) {
        const std::vector<NodeParameter> parameters {
                { "enabled", "Enabled", "1" },
                { "size", "Size", "0" },
                { "damp", "Damp", "0.2" },
                { "width", "Width", String(width) },
                { "wet", "Wet", "0.4" },
                { "highPass", "High Pass", "0.05" }
        };
        const auto configuration = ReverbSignalProcessor::buildConfiguration(parameters);
        const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
        PreviewProcessContext context;
        context.pointCount = 24;
        context.parameters = parameters;
        context.configuration = &published;
        auto processor = NodePreviewProcessorFactory().create(
                PreviewModuleRole::ReverbSpectrogram);
        processor->render(context);
        return context;
    };
    const auto difference = [](const PreviewProcessContext& context) {
        double total {};
        for (size_t index = 0; index < context.primary.size(); ++index) {
            total += std::abs(
                    context.primary[index]
                    - context.secondary[index]);
        }
        return total;
    };

    const auto mono = render(0.5f);
    const auto stereo = render(1.f);
    REQUIRE(difference(mono) == Catch::Approx(0.0).margin(1.0e-6));
    REQUIRE(difference(stereo) > 1.0);
}

TEST_CASE("Reverb high pass visibly attenuates lower spectrogram partials",
        "[cycle-v2][runtime][effects][reverb][preview][ui]") {
    auto renderImage = [](float damping, float highPass) {
        const std::vector<NodeParameter> parameters {
                { "enabled", "Enabled", "1" },
                { "size", "Size", "0.5" },
                { "damp", "Damp", String(damping) },
                { "width", "Width", "1" },
                { "wet", "Wet", "1" },
                { "highPass", "High Pass", String(highPass) }
        };
        const auto configuration = ReverbSignalProcessor::buildConfiguration(parameters);
        const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
        PreviewProcessContext context;
        context.pointCount = 40;
        context.parameters = parameters;
        context.configuration = &published;
        auto processor = NodePreviewProcessorFactory().create(
                PreviewModuleRole::ReverbSpectrogram);
        processor->render(context);

        NodePreviewResult result;
        result.role = PreviewModuleRole::ReverbSpectrogram;
        result.primary = std::move(context.primary);
        result.secondary = std::move(context.secondary);
        result.gridColumns = context.gridColumns;
        result.gridRows = context.gridRows;
        result.domain = context.domain;
        return NodePreviewRenderer::createRuntimeHeatmapImage(result);
    };

    auto bandBrightness = [](const Image& image, int firstRow, int rowCount) {
        double brightness {};
        for (int y = firstRow; y < firstRow + rowCount; ++y) {
            for (int x = 0; x < image.getWidth(); ++x) {
                brightness += image.getPixelAt(x, y).getPerceivedBrightness();
            }
        }
        return brightness / (double) (image.getWidth() * rowCount);
    };

    for (const float damping : { 0.f, 1.f }) {
        CAPTURE(damping);
        const Image unfiltered = renderImage(damping, 0.f);
        const Image highPassed = renderImage(damping, 1.f);
        REQUIRE(unfiltered.isValid());
        REQUIRE(highPassed.isValid());
        REQUIRE(highPassed.getBounds() == unfiltered.getBounds());

        const int bandHeight = unfiltered.getHeight() / 3;
        const double unfilteredHigh = bandBrightness(unfiltered, 0, bandHeight);
        const double highPassedHigh = bandBrightness(highPassed, 0, bandHeight);
        const double unfilteredLow = bandBrightness(
                unfiltered,
                unfiltered.getHeight() - bandHeight,
                bandHeight);
        const double highPassedLow = bandBrightness(
                highPassed,
                highPassed.getHeight() - bandHeight,
                bandHeight);
        const double highRetention = highPassedHigh / unfilteredHigh;
        const double lowRetention = highPassedLow / unfilteredLow;
        INFO("high-band retention: " << highRetention);
        INFO("low-band retention: " << lowRetention);
        REQUIRE(highRetention > 0.9);
        REQUIRE(highRetention < 1.15);
        REQUIRE(lowRetention < 0.85);
        REQUIRE(lowRetention < highRetention - 0.2);
    }
}

TEST_CASE("Spy preview processor requires a traversal grid", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    auto processor = factory.create(PreviewModuleRole::SignalSpy);
    REQUIRE(processor != nullptr);

    PreviewProcessContext emptyContext;
    emptyContext.pointCount = 4;
    processor->render(emptyContext);

    REQUIRE(emptyContext.primary.empty());
    REQUIRE(emptyContext.secondary.empty());
    REQUIRE(emptyContext.gridColumns == 0);
    REQUIRE(emptyContext.gridRows == 0);

    PreviewProcessContext gridContext;
    const std::vector<float> inputGrid { 0.f, 0.25f, 0.5f, 0.75f };
    gridContext.input.grid = inputGrid.data();
    gridContext.input.gridSize = inputGrid.size();
    gridContext.input.gridColumns = 2;
    gridContext.input.gridRows = 2;
    gridContext.domain = PortDomain::SpectralMagnitudeSignal;
    processor->render(gridContext);

    REQUIRE(gridContext.primary == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f });
    REQUIRE(gridContext.secondary.empty());
    REQUIRE(gridContext.gridColumns == 2);
    REQUIRE(gridContext.gridRows == 2);
    REQUIRE(gridContext.domain == PortDomain::SpectralMagnitudeSignal);
}

TEST_CASE("Waveform preview processor produces normalized summary points", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    auto processor = factory.create(PreviewModuleRole::Waveform);
    REQUIRE(processor != nullptr);

    PreviewProcessContext context;
    context.pointCount = 5;
    processor->render(context);

    REQUIRE(context.primary == std::vector<float> { 0.f, 0.5f, 1.f, 0.5f, 0.f });
    REQUIRE(context.secondary == std::vector<float> { 1.f, 0.5f, 0.f, 0.5f, 1.f });
}

TEST_CASE("Preview processors read node parameters from process context", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    PreviewProcessContext context;
    context.pointCount = 3;
    context.parameters = { { "amplitude", "Amplitude", "0.5" } };

    factory.create(PreviewModuleRole::Waveform)->render(context);

    REQUIRE(context.primary == std::vector<float> { 0.f, 0.5f, 0.f });
    REQUIRE(context.secondary == std::vector<float> { 1.f, 0.5f, 1.f });
}

TEST_CASE("Preview processors cover mesh, image, and output summaries", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;

    PreviewProcessContext mesh;
    mesh.pointCount = 4;
    factory.create(PreviewModuleRole::MeshSurface)->render(mesh);
    REQUIRE(mesh.primary.size() == 32);
    REQUIRE(mesh.secondary.size() == 4);
    REQUIRE(mesh.gridColumns == 8);
    REQUIRE(mesh.gridRows == 4);
    REQUIRE(mesh.domain == PortDomain::TimeSignal);
    REQUIRE(*std::min_element(mesh.primary.begin(), mesh.primary.end()) >= -1.f);
    REQUIRE(*std::max_element(mesh.primary.begin(), mesh.primary.end()) <= 1.f);

    PreviewProcessContext image;
    image.pointCount = 4;
    factory.create(PreviewModuleRole::Image)->render(image);
    REQUIRE(image.primary == std::vector<float> { 0.f, 1.f / 3.f, 2.f / 3.f, 1.f });

    PreviewProcessContext meters;
    meters.pointCount = 3;
    factory.create(PreviewModuleRole::OutputMeters)->render(meters);
    REQUIRE(meters.primary == std::vector<float> { 0.65f, 0.65f, 0.65f });
    REQUIRE(meters.secondary == std::vector<float> { 0.62f, 0.62f, 0.62f });
}

TEST_CASE("Preview processors can reflect upstream summaries", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;

    PreviewProcessContext meters;
    meters.pointCount = 2;
    const std::vector<float> meterSummary { 0.2f, 0.6f };
    meters.input.summary = &meterSummary;
    factory.create(PreviewModuleRole::OutputMeters)->render(meters);

    REQUIRE(meters.primary == std::vector<float> { 0.4f, 0.4f });
    REQUIRE(meters.secondary.size() == 2);
    REQUIRE(meters.secondary[0] == Catch::Approx(0.38f));

    PreviewProcessContext mesh;
    mesh.pointCount = 3;
    const std::vector<float> meshSummary { 0.1f, 0.9f };
    mesh.input.summary = &meshSummary;
    factory.create(PreviewModuleRole::MeshSurface)->render(mesh);

    REQUIRE(mesh.secondary.size() == 3);
    REQUIRE(mesh.primary.size() == 24);
}

TEST_CASE("Waveshaper preview processor produces a transfer curve", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    PreviewProcessContext context;
    context.pointCount = 3;

    factory.create(PreviewModuleRole::Waveshaper)->render(context);

    REQUIRE(context.primary == std::vector<float> { 0.f, 0.25f, 1.f });
    REQUIRE(context.secondary == std::vector<float> { 0.f, 0.5f, 1.f });
}
