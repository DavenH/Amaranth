#include <catch2/catch_test_macros.hpp>

#include <array>

#include "Graph/NodeGraph.h"
#include "Runtime/SpectralFrameGraphCombiner.h"

using namespace CycleV2;

TEST_CASE("Spectral frame combiner owns one-sided and transferred binary policy",
        "[cycle-v2][runtime][spectral-frame][combiner]") {
    constexpr int valueCount = 5;
    constexpr int activeHarmonicCount = valueCount - 1;
    CycleDsp::SpectralStageCaptureRecorder recorder;
    REQUIRE(recorder.prepare(valueCount, 0));
    const CycleDsp::SpectralFrameCapture capture(&recorder, 0, 0, 48);
    const SpectralFrameGraphCombiner combiner(capture, activeHarmonicCount);

    std::array<float, valueCount> right { 0.f, 0.2f, -0.4f, 0.6f, 0.8f };
    std::array<float, valueCount> output {};
    std::array<float, valueCount> scratch {};
    combiner.add(
            {},
            { { right.data(), valueCount }, nullptr, true },
            { output.data(), valueCount },
            { scratch.data(), valueCount },
            PortDomain::SpectralMagnitudeSignal,
            0);
    REQUIRE(output == std::array<float, valueCount> { 0.f, 0.2f, 0.f, 0.6f, 0.8f });

    std::array<float, valueCount> left { 0.f, 0.1f, 0.2f, 0.3f, 0.4f };
    SpectralMagnitudeTransfer disabledMultiply;
    disabledMultiply.mode = SpectralMagnitudeTransferMode::MultiplyUnipolar;
    disabledMultiply.enabled = false;
    combiner.multiply(
            { { left.data(), valueCount }, nullptr, true },
            { { right.data(), valueCount }, &disabledMultiply, true },
            { output.data(), valueCount },
            { scratch.data(), valueCount },
            PortDomain::SpectralMagnitudeSignal,
            0);
    REQUIRE(output == left);
    const auto* captured = recorder.record(
            CycleDsp::SpectralStage::MagnitudeOperand, 0);
    REQUIRE(captured != nullptr);
    REQUIRE(captured->primary.size() == activeHarmonicCount);
}
