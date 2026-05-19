#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/FFT/FftGridwiseDsp.h"

using namespace CycleV2;

namespace {

AudioProcessBlock block(std::initializer_list<float> samples) {
    return { std::vector<float>(samples), PortDomain::TimeSignal, ChannelLayout::LinkedStereo };
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
    REQUIRE(columns[0].magnitude.samples.size() == 4);
    REQUIRE(columns[1].magnitude.samples.size() == 4);
    REQUIRE(columns[0].magnitude.samples[0] == columns[1].magnitude.samples[0]);
}
