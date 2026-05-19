#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodePreviewProcessor.h"

using namespace CycleV2;

TEST_CASE("Node preview processor factory creates preview modules", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;

    auto processor = factory.create(PreviewModuleRole::MeshSurface);
    REQUIRE(processor != nullptr);
    REQUIRE(processor->role() == PreviewModuleRole::MeshSurface);

    REQUIRE(factory.create(PreviewModuleRole::None) == nullptr);
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

TEST_CASE("Preview processors cover mesh, image, and output summaries", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;

    PreviewProcessContext mesh;
    mesh.pointCount = 4;
    factory.create(PreviewModuleRole::MeshSurface)->render(mesh);
    REQUIRE(mesh.primary == std::vector<float> { 0.25f, 0.75f, 0.25f, 0.75f });

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

TEST_CASE("Waveshaper preview processor produces a transfer curve", "[cycle-v2][runtime]") {
    NodePreviewProcessorFactory factory;
    PreviewProcessContext context;
    context.pointCount = 3;

    factory.create(PreviewModuleRole::Waveshaper)->render(context);

    REQUIRE(context.primary == std::vector<float> { 0.f, 0.25f, 1.f });
    REQUIRE(context.secondary == std::vector<float> { 0.f, 0.5f, 1.f });
}
