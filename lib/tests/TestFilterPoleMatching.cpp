#include <catch2/catch_test_macros.hpp>

#include <Audio/Filters/Biquad.h>
#include <Audio/Filters/Types.h>

namespace {
    class ExposedBiquad : public Dsp::BiquadBase {
    public:
        using Dsp::BiquadBase::setTwoPole;
    };
}

TEST_CASE("Filter pole assertions allow numerically conjugate EQ pairs", "[filters]") {
    const Dsp::complex_t pole1(0.98713646615210803, -0.030562545525753479);
    const Dsp::complex_t pole2(0.98713646615210803 + 1e-15, 0.030562545525753479);
    const Dsp::complex_t zero1(0.98713646615210803, -0.030562545525753479);
    const Dsp::complex_t zero2(0.98713646615210803 - 1e-15, 0.030562545525753479);

    REQUIRE(Dsp::ComplexPair(pole1, pole2).isMatchedPair());
    REQUIRE(Dsp::ComplexPair(zero1, zero2).isMatchedPair());

    ExposedBiquad biquad;
    biquad.setTwoPole(pole1, zero1, pole2, zero2);
}
