#include <algorithm>
#include <numeric>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodePreviewProcessor.h"
#include "../src/Nodes/Effects/EffectPreviewRenderer.h"
#include "../src/Nodes/Effects/EffectSignalProcessors.h"
#include "../src/UI/NodePreviewRenderer.h"

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

TEST_CASE("Disabled compact effect previews are greyscale",
        "[cycle-v2][effects][preview]") {
    for (const NodeKind kind : { NodeKind::Delay, NodeKind::Reverb, NodeKind::Equalizer }) {
        Node enabled;
        enabled.kind = kind;
        enabled.parameters.push_back({ "enabled", "Enabled", "1" });
        Node disabled = enabled;
        disabled.parameters.front().value = "0";

        Image enabledImage(Image::RGB, 200, 60, true);
        Graphics enabledGraphics(enabledImage);
        REQUIRE(paintEffectCompactPreview(
                enabledGraphics,
                enabledImage.getBounds().toFloat(),
                enabled,
                1.f));

        Image disabledImage(Image::RGB, 200, 60, true);
        Graphics disabledGraphics(disabledImage);
        REQUIRE(paintEffectCompactPreview(
                disabledGraphics,
                disabledImage.getBounds().toFloat(),
                disabled,
                1.f));

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
    paintEqualizerResponsePreview(
            enabledGraphics,
            enabledImage.getBounds().toFloat(),
            enabled,
            true);

    Image disabledImage(Image::RGB, 500, 120, true);
    Graphics disabledGraphics(disabledImage);
    paintEqualizerResponsePreview(
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
    gridContext.input.grid = &inputGrid;
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
    REQUIRE(mesh.primary[0] >= 0.f);
    REQUIRE(mesh.primary[0] <= 1.f);

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
