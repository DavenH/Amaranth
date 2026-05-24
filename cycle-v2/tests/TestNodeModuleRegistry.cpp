#include <catch2/catch_test_macros.hpp>

#include "../src/Runtime/NodeModuleRegistry.h"

using namespace CycleV2;

TEST_CASE("Node module registry describes source nodes", "[cycle-v2][runtime]") {
    const NodeModuleRegistry registry;

    const auto voice = registry.descriptorFor(NodeKind::VoiceContext);
    REQUIRE(voice.audioRole == AudioModuleRole::VoiceContext);
    REQUIRE(voice.previewRole == PreviewModuleRole::VoiceContext);
    REQUIRE(voice.executable);
    REQUIRE_FALSE(voice.previewable);

    const auto wave = registry.descriptorFor(NodeKind::WaveSource);
    REQUIRE(wave.audioRole == AudioModuleRole::WaveSource);
    REQUIRE(wave.previewRole == PreviewModuleRole::Waveform);
    REQUIRE(wave.previewable);

    const auto image = registry.descriptorFor(NodeKind::ImageSource);
    REQUIRE(image.audioRole == AudioModuleRole::ImageSource);
    REQUIRE(image.previewRole == PreviewModuleRole::Image);
    REQUIRE(image.previewable);
}

TEST_CASE("Node module registry separates audio and previewless utility nodes", "[cycle-v2][runtime]") {
    const NodeModuleRegistry registry;

    const auto fft = registry.descriptorFor(NodeKind::Fft);
    REQUIRE(fft.audioRole == AudioModuleRole::Fft);
    REQUIRE(fft.previewRole == PreviewModuleRole::None);
    REQUIRE_FALSE(fft.previewable);

    const auto ifft = registry.descriptorFor(NodeKind::Ifft);
    REQUIRE(ifft.audioRole == AudioModuleRole::Ifft);
    REQUIRE(ifft.previewRole == PreviewModuleRole::None);
    REQUIRE_FALSE(ifft.previewable);

    const auto multiply = registry.descriptorFor(NodeKind::Multiply);
    REQUIRE(multiply.audioRole == AudioModuleRole::Multiply);
    REQUIRE(multiply.previewRole == PreviewModuleRole::None);
    REQUIRE_FALSE(multiply.previewable);

    const auto output = registry.descriptorFor(NodeKind::Output);
    REQUIRE(output.audioRole == AudioModuleRole::Output);
    REQUIRE(output.previewRole == PreviewModuleRole::None);
    REQUIRE_FALSE(output.previewable);
}

TEST_CASE("Node module registry marks Cycle 1 adapter-backed modules", "[cycle-v2][runtime]") {
    const NodeModuleRegistry registry;

    REQUIRE(registry.descriptorFor(NodeKind::TrilinearMesh).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::TrilinearMesh).cycle1Reference
            == "cycle/src/Curve/Rasterization/Rasterizer/VoiceMeshRasterizer.cpp");
    REQUIRE(registry.descriptorFor(NodeKind::Envelope).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::Envelope).cycle1Reference
            == "cycle/src/Inter/EnvelopeInter2D.cpp");
    REQUIRE(registry.descriptorFor(NodeKind::ImpulseResponse).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::Waveshaper).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::Delay).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::Delay).cycle1Reference
            == "cycle/src/Audio/Effects/Delay.cpp");
    REQUIRE_FALSE(registry.descriptorFor(NodeKind::ImageSource).cycle1AdapterBacked);
    REQUIRE(registry.descriptorFor(NodeKind::ImageSource).cycle1Reference.isEmpty());
}

TEST_CASE("Node module role labels are stable", "[cycle-v2][runtime]") {
    REQUIRE(labelForAudioModuleRole(AudioModuleRole::ImageSource) == "Image Source");
    REQUIRE(labelForPreviewModuleRole(PreviewModuleRole::MeshSurface) == "Mesh Surface");
}
