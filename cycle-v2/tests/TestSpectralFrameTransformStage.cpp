#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

#include "Runtime/SpectralFrameTransformStage.h"

using namespace CycleV2;

TEST_CASE("Spectral frame transform stage preserves capture and reconstruction",
        "[cycle-v2][runtime][spectral-frame][transform]") {
    constexpr int frameSize = 16;
    constexpr int activeHarmonicCount = frameSize / 2 - 1;
    SpectralFrameTransformStage stage;
    REQUIRE_FALSE(stage.prepare(3));
    REQUIRE(stage.prepare(frameSize));
    Transform* transform = stage.transformFor(frameSize);
    REQUIRE(transform != nullptr);
    REQUIRE(stage.transformFor(frameSize * 2) == nullptr);

    std::vector<float> time((size_t) frameSize);
    std::vector<float> expected((size_t) frameSize);
    for (int index = 0; index < frameSize; ++index) {
        time[(size_t) index] = std::sin(
                MathConstants<float>::twoPi * 2.f * (float) index / frameSize);
    }
    expected = time;
    std::vector<float> magnitude((size_t) frameSize / 2 + 1);
    std::vector<float> phase(magnitude.size());
    std::vector<float> reconstructed((size_t) frameSize);

    CycleDsp::SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(frameSize, 0));
    const CycleDsp::SpectralFrameCapture capture(&recorder, 0, 0, 48);
    stage.forward(
            *transform,
            { time.data(), frameSize },
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() },
            activeHarmonicCount,
            capture,
            0);
    stage.inverse(
            *transform,
            { magnitude.data(), (int) magnitude.size() },
            { phase.data(), (int) phase.size() },
            { reconstructed.data(), frameSize },
            activeHarmonicCount,
            false,
            capture,
            0);

    const auto* forward = recorder.record(CycleDsp::SpectralStage::ForwardFft, 0);
    const auto* postLayer = recorder.record(
            CycleDsp::SpectralStage::PostLayerSpectrum, 0);
    const auto* inverse = recorder.record(
            CycleDsp::SpectralStage::ReconstructedFrame, 0);
    REQUIRE(forward != nullptr);
    REQUIRE(postLayer != nullptr);
    REQUIRE(inverse != nullptr);
    REQUIRE(forward->primary.size() == activeHarmonicCount);
    REQUIRE(postLayer->primary.size() == activeHarmonicCount);
    REQUIRE(inverse->primary.size() == frameSize);
    for (int index = 0; index < frameSize; ++index) {
        REQUIRE(reconstructed[(size_t) index]
                == Catch::Approx(expected[(size_t) index]).margin(1.0e-5f));
    }
}
