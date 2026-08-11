#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Util/LogRegionMapping.h>

#include <cmath>
#include <vector>

TEST_CASE("Log region mapping preserves Cycle pitch-dependent frequency coordinates",
        "[util][spectral][log-regions]") {
    const LogRegionMapping c3(48);
    const LogRegionMapping c5(72);

    REQUIRE(c3.regionSize() == 338);
    REQUIRE(c5.regionSize() == 85);

    std::vector<float> c3Ramp((size_t) c3.regionSize());
    std::vector<float> c5Ramp((size_t) c5.regionSize());
    c3.fillDisplayUnits(Buffer<float>(c3Ramp.data(), (int) c3Ramp.size()));
    c5.fillDisplayUnits(Buffer<float>(c5Ramp.data(), (int) c5Ramp.size()));

    CHECK(c3Ramp.front() == Catch::Approx(0.05f).margin(1.0e-6f));
    CHECK(c3Ramp.back() == Catch::Approx(1.f).margin(1.0e-6f));
    CHECK(c5Ramp.front() == Catch::Approx(0.05f).margin(1.0e-6f));
    CHECK(c5Ramp.back() == Catch::Approx(1.f).margin(1.0e-6f));
    CHECK(c3.displayUnitForSourceUnit(0.3f)
            > c5.displayUnitForSourceUnit(0.3f));
}

TEST_CASE("Log region display and source coordinates are inverse mappings",
        "[util][spectral][log-regions]") {
    constexpr int count = 257;
    const LogRegionMapping mapping(60);
    std::vector<float> display((size_t) count);
    std::vector<float> source((size_t) count);
    mapping.fillDisplayUnits(Buffer<float>(display.data(), count));
    mapping.fillSourceUnits(Buffer<float>(source.data(), count));

    for (int index = 1; index < count; ++index) {
        CHECK(display[(size_t) index] >= display[(size_t) index - 1]);
        CHECK(source[(size_t) index] >= source[(size_t) index - 1]);
        if (index >= 13) {
            CHECK(mapping.displayUnitForSourceUnit(source[(size_t) index])
                    == Catch::Approx((float) index / (float) (count - 1))
                            .margin(1.0e-5f));
        }
    }
    CHECK(source.front() == Catch::Approx(0.f));
    CHECK(source.back() == Catch::Approx(1.f));
}
