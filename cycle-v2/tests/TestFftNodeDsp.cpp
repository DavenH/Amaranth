#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Nodes/FFT/FftBlockwiseDsp.h"
#include "../src/Nodes/FFT/FftGridwiseDsp.h"

using namespace CycleV2;

namespace {

AudioProcessBlock block(std::initializer_list<float> samples) {
    return { std::vector<float>(samples) };
}

}

TEST_CASE("FFT gridwise DSP renders independent cycle columns", "[cycle-v2][nodes][fft]") {
    FftGridwiseDsp dsp;

    const auto columns = dsp.forwardColumns({
            block({ -0.5f, -0.25f, 0.25f, 0.5f }),
            block({ 0.5f, 0.25f, -0.25f, -0.5f })
    });

    REQUIRE(columns.size() == 2);
    REQUIRE(columns[0].magnitude.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(columns[0].phase.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(columns[0].magnitude.block.samples.size() == 3);
    REQUIRE(columns[1].magnitude.block.samples.size() == 3);
    REQUIRE(columns[0].magnitude.block.samples[0] == columns[1].magnitude.block.samples[0]);
}

TEST_CASE("FFT blockwise inverse applies half-cycle carry after first cycle", "[cycle-v2][nodes][fft]") {
    FftBlockwiseDsp dsp;

    AudioProcessBlock first = block({ -0.5f, -0.25f, 0.25f, 0.5f });
    AudioProcessBlock firstMag;
    AudioProcessBlock firstPhase;
    dsp.forward(first, firstMag, firstPhase);

    AudioProcessBlock firstOut;
    firstOut.samples.resize(4);
    dsp.inverse(firstMag, &firstPhase, firstOut);

    AudioProcessBlock second = block({ 0.5f, 0.25f, -0.25f, -0.5f });
    AudioProcessBlock secondMag;
    AudioProcessBlock secondPhase;
    dsp.forward(second, secondMag, secondPhase);

    AudioProcessBlock secondOut;
    secondOut.samples.resize(4);
    dsp.inverse(secondMag, &secondPhase, secondOut);

    REQUIRE(firstOut.samples[0] == Catch::Approx(-0.5f).margin(1.0e-5f));
    REQUIRE(firstOut.samples[1] == Catch::Approx(-0.25f).margin(1.0e-5f));
    REQUIRE(secondOut.samples[0] == Catch::Approx(-0.5f).margin(1.0e-5f));
    REQUIRE(secondOut.samples[1] == Catch::Approx(0.25f).margin(1.0e-5f));
    REQUIRE(secondOut.samples[2] == Catch::Approx(-0.25f).margin(1.0e-5f));
    REQUIRE(secondOut.samples[3] == Catch::Approx(-0.5f).margin(1.0e-5f));
}

TEST_CASE("FFT blockwise inverse can round trip a cycle without carry", "[cycle-v2][nodes][fft]") {
    FftBlockwiseDsp dsp;
    dsp.setHalfCycleCarryEnabled(false);

    AudioProcessBlock input = block({ 0.120191f, 0.25f, -0.1f, 0.5f, -0.35f, 0.2f, -0.05f, -0.134082f });
    AudioProcessBlock magnitude;
    AudioProcessBlock phase;
    dsp.forward(input, magnitude, phase);

    AudioProcessBlock output;
    output.samples.resize(input.samples.size());
    dsp.inverse(magnitude, &phase, output);

    REQUIRE(output.samples.size() == input.samples.size());
    for (size_t i = 0; i < input.samples.size(); ++i) {
        INFO("sample: " << i);
        REQUIRE(output.samples[i] == Catch::Approx(input.samples[i]).margin(1.0e-5f));
    }
}
