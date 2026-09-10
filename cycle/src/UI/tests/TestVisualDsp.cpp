#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "../VisualDsp.h"

class VisualDspColumnCopyTest {
public:
    static void copy(
            const std::vector<Column>& source,
            std::vector<Column>& destination) {
        VisualDsp::copyArrayOrParts(source, destination);
    }
};

TEST_CASE("VisualDsp copies columns into destination-owned resolutions",
        "[cycle][visual-dsp][columns]") {
    std::array<float, 8> sourceValues { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
    std::array<float, 4> destinationValues {};
    std::vector<Column> source {
            Column(sourceValues.data(), (int) sourceValues.size(), 0.f, 60)
    };
    std::vector<Column> destination {
            Column(destinationValues.data(), (int) destinationValues.size(), 0.f, 60)
    };

    VisualDspColumnCopyTest::copy(source, destination);

    REQUIRE(destination.front().size() == 4);
    REQUIRE(destinationValues == std::array<float, 4> { 1.f, 3.f, 5.f, 7.f });
}
