#include <algorithm>
#include <numeric>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodePreviewProcessor.h"
#include "../src/Nodes/Effects/EffectSignalProcessors.h"

using namespace CycleV2;

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

TEST_CASE("Reverb preview spectrogram analyzes a convolved Dirac response",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    NodePreviewProcessorFactory factory;
    auto processor = factory.create(PreviewModuleRole::ReverbSpectrogram);
    REQUIRE(processor != nullptr);

    const auto configuration = ReverbSignalProcessor::buildConfiguration({
            { "enabled", "Enabled", "1" },
            { "size", "Size", "0" },
            { "damp", "Damp", "0.2" },
            { "width", "Width", "1" },
            { "wet", "Wet", "0.8" },
            { "highPass", "High Pass", "0.05" }
    });
    const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
    PreviewProcessContext context;
    context.pointCount = 24;
    context.configuration = &published;

    processor->render(context);

    REQUIRE(context.gridColumns == 24);
    REQUIRE(context.gridRows == 24);
    REQUIRE(context.primary.size() == context.gridColumns * context.gridRows);
    REQUIRE(context.secondary.empty());
    REQUIRE(context.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(*std::max_element(context.primary.begin(), context.primary.end()) > 0.f);
}

TEST_CASE("Reverb spectrogram preserves wet high-pass and size semantics",
        "[cycle-v2][runtime][effects][reverb][preview]") {
    auto render = [](float size, float damp, float highPass, float wet) {
        const auto configuration = ReverbSignalProcessor::buildConfiguration({
                { "enabled", "Enabled", "1" },
                { "size", "Size", String(size) },
                { "damp", "Damp", String(damp) },
                { "width", "Width", "1" },
                { "wet", "Wet", String(wet) },
                { "highPass", "High Pass", String(highPass) }
        });
        const PublishedNodeConfiguration published { 1, "reverb-preview", configuration };
        PreviewProcessContext context;
        context.pointCount = 24;
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
    auto lowBandEnergy = [](const PreviewProcessContext& context) {
        constexpr size_t lowBandRows = 4;
        float energy {};
        for (size_t column = 0; column < context.gridColumns; ++column) {
            const auto first = context.primary.begin()
                    + (std::vector<float>::difference_type) (column * context.gridRows);
            energy += std::accumulate(first, first + lowBandRows, 0.f);
        }
        return energy;
    };
    const float unfilteredLowEnergy = lowBandEnergy(highWet);
    const float highPassedLowEnergy = lowBandEnergy(highPassed);
    REQUIRE(highPassedLowEnergy < unfilteredLowEnergy);

    const auto largeRoom = render(1.f, 1.f, 0.f, 1.f);
    REQUIRE(largeRoom.gridColumns > highWet.gridColumns);
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
