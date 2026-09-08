#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <Array/ScopedAlloc.h>
#include <Audio/WaveshaperTransfer.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Rasterization/Rasterizer/Rasterizer2D.h>

namespace {
    struct CurveTableScope {
        CurveTableScope() {
            Curve::calcTable();
        }

        ~CurveTableScope() {
            Curve::deleteTable();
        }
    };

    std::vector<Intercept> makeFuzzBassWaveshaper() {
        // Exact migrated curve from the shipped FuzzBass.cyc preset.
        return {
            Intercept(0.129621476f, 0.125000000f, nullptr, 0.767164588f),
            Intercept(0.218750000f, 0.218750000f, nullptr, 1.000000000f),
            Intercept(0.781250000f, 0.781250000f, nullptr, 1.000000000f),
            Intercept(0.868265867f, 0.784106970f, nullptr, 0.000000000f),
            Intercept(0.659507036f, 0.788656890f, nullptr, 0.221888483f),
        };
    }
}

TEST_CASE("FuzzBass waveshaper transfer has no isolated sample spikes", "[waveshaper]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makeFuzzBassWaveshaper();
    Rasterizer2D rasterizer(points, false);
    rasterizer.updateWaveform();

    Rasterization::SamplerView sampler(
            rasterizer.result().waveform,
            rasterizer.result().sampleable);
    WaveshaperTransfer transfer;
    transfer.rasterizeFrom(sampler, 0.125f);

    float largestSecondDifference = 0.f;
    int largestIndex = 0;

    for (int i = 1; i < WaveshaperTransfer::tableResolution - 1; ++i) {
        const float previous = transfer.lookup(float(i - 1) / float(WaveshaperTransfer::tableResolution - 1));
        const float current = transfer.lookup(float(i) / float(WaveshaperTransfer::tableResolution - 1));
        const float next = transfer.lookup(float(i + 1) / float(WaveshaperTransfer::tableResolution - 1));
        const float secondDifference = std::abs(next - 2.f * current + previous);

        if (secondDifference > largestSecondDifference) {
            largestSecondDifference = secondDifference;
            largestIndex = i;
        }
    }

    INFO("largest index=" << largestIndex);
    INFO("largest second difference=" << largestSecondDifference);
    REQUIRE(largestSecondDifference < 0.01f);

    constexpr int sampleCount = 4800;
    ScopedAlloc<float> signalMemory(sampleCount);
    Buffer<float> signal(signalMemory, sampleCount);
    signal.sin(100.f / 48000.f);
    transfer.process(signal, 1.f, 1.f);

    float signalSecondDifference = 0.f;
    for (int i = 1; i < signal.size() - 1; ++i) {
        signalSecondDifference = jmax(
                signalSecondDifference,
                std::abs(signal[i + 1] - 2.f * signal[i] + signal[i - 1]));
    }

    REQUIRE(signalSecondDifference < 0.01f);
}
