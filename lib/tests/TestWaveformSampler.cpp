#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/RasterizerViews.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>

namespace {
    struct LinearWaveform {
        LinearWaveform() {
            waveform.place(memory, 5);
            for (int i = 0; i < 5; ++i) {
                const float x = -1.f + (float) i;
                waveform.waveX[i] = x;
                waveform.waveY[i] = 2.f * x + 1.f;
                waveform.diffX[i] = i < 4 ? 1.f : 0.f;
                waveform.slope[i] = i < 4 ? 2.f : 0.f;
                waveform.area[i] = i < 4
                        ? 0.5f * (waveform.waveY[i] + (2.f * (x + 1.f) + 1.f))
                        : 0.f;
            }
            waveform.zeroIndex = 1;
            waveform.oneIndex = 2;
        }

        ScopedAlloc<float> memory;
        Rasterization::WaveformBuffers waveform;
    };
}

TEST_CASE("Waveform scalar sampling has explicit half-open bounds", "[rasterization][sampling]") {
    LinearWaveform linear;
    int cursor = -7;

    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, false, -1., cursor) == Catch::Approx(-1.f));
    REQUIRE(cursor == 1);
    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, false, 0.25, cursor) == Catch::Approx(1.5f));
    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, false, std::nextafter(3.f, 0.f), cursor) == Catch::Approx(7.f));

    cursor = 99;
    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, false, 3., cursor) == Catch::Approx(0.f));
    REQUIRE(cursor == 0);
    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, false, std::nextafter(-1.f, -2.f), cursor) == Catch::Approx(0.f));
    REQUIRE(Rasterization::WaveformSampler::sampleAt(
            linear.waveform, true, 0.25, cursor) == Catch::Approx(0.f));
}

TEST_CASE("Waveform bulk samplers agree for an analytic linear waveform", "[rasterization][sampling]") {
    LinearWaveform linear;
    Rasterization::SamplerView sampler(linear.waveform, true);
    ScopedAlloc<float> memory(12);
    Buffer<float> intervals = memory.place(3);
    Buffer<float> direct = memory.place(3);
    Buffer<float> even = memory.place(3);
    Buffer<float> integrated = memory.place(3);
    intervals[0] = 0.f;
    intervals[1] = 0.5f;
    intervals[2] = 1.f;

    sampler.sampleAtIntervals(intervals, direct);
    const double nextPhase = sampler.sampleWithInterval(even, 0.5, 0.);
    const float nextIntegratedPhase = sampler.samplePerfectly(0.5, integrated, 0.25);

    for (int i = 0; i < 3; ++i) {
        REQUIRE(direct[i] == Catch::Approx(1.f + (float) i));
        REQUIRE(even[i] == Catch::Approx(direct[i]));
        REQUIRE(integrated[i] == Catch::Approx(1.5f + (float) i));
    }
    REQUIRE(nextPhase == Catch::Approx(0.5));
    REQUIRE(nextIntegratedPhase == Catch::Approx(-0.25f));
}

TEST_CASE("Waveform bulk sampling fails safely and consistently", "[rasterization][sampling]") {
    LinearWaveform linear;
    ScopedAlloc<float> memory(9);
    Buffer<float> intervals = memory.place(3);
    Buffer<float> output = memory.place(3);
    intervals[0] = 0.f;
    intervals[1] = 1.f;
    intervals[2] = 0.5f;
    output.set(9.f);

    Rasterization::WaveformSampler::sampleAtIntervals(linear.waveform, intervals, output);
    REQUIRE(output[0] == 0.f);
    REQUIRE(output[1] == 0.f);
    REQUIRE(output[2] == 0.f);

    output.set(9.f);
    REQUIRE(Rasterization::WaveformSampler::sampleWithInterval(
            linear.waveform, output, 0., 0.25) == Catch::Approx(0.25));
    REQUIRE(output[0] == 0.f);
    REQUIRE(output[1] == 0.f);
    REQUIRE(output[2] == 0.f);

    output.set(9.f);
    REQUIRE(Rasterization::WaveformSampler::sampleWithInterval(
            linear.waveform, output, -0.5, 0.25) == Catch::Approx(0.25));
    REQUIRE(output[0] == 0.f);
    REQUIRE(output[1] == 0.f);
    REQUIRE(output[2] == 0.f);

    Rasterization::WaveformBuffers empty;
    Rasterization::SamplerView unavailable(empty, false);
    output.set(9.f);
    REQUIRE(unavailable.samplePerfectly(0.5, output, 0.25) == Catch::Approx(0.25f));
    REQUIRE(output[0] == 0.f);
    REQUIRE(output[1] == 0.f);
    REQUIRE(output[2] == 0.f);

    Buffer<float> noOutput;
    REQUIRE(unavailable.sampleWithInterval(noOutput, 0.5, 0.25) == Catch::Approx(0.25));
}

TEST_CASE("One-point waveforms are not sampleable", "[rasterization][sampling]") {
    ScopedAlloc<float> memory(5);
    Rasterization::WaveformBuffers onePoint;
    onePoint.place(memory, 1);
    onePoint.waveX[0] = 0.f;
    onePoint.waveY[0] = 1.f;

    REQUIRE_FALSE(Rasterization::WaveformSampler::isSampleable(onePoint));
    REQUIRE(Rasterization::WaveformSampler::sampleAt(onePoint, false, 0.) == 0.f);
}
