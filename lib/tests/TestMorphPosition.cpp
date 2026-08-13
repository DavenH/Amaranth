#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Obj/MorphPosition.h"

TEST_CASE("MorphPosition resetTime pins current and target values", "[morph]") {
    MorphPosition position(0.75f, 0.25f, 0.5f);

    position.resetTime();

    REQUIRE(position.time.getCurrentValue() == Catch::Approx(0.f));
    REQUIRE(position.time.getTargetValue() == Catch::Approx(0.f));
    REQUIRE(position.red.getCurrentValue() == Catch::Approx(0.25f));
    REQUIRE(position.blue.getCurrentValue() == Catch::Approx(0.5f));
}

TEST_CASE("MorphPosition companion dimensions follow VertCube face coordinates", "[morph][mesh]") {
    int dimX = -1;
    int dimY = -1;

    MorphPosition::getOtherDims(Vertex::Time, dimX, dimY);
    REQUIRE(dimX == Vertex::Red);
    REQUIRE(dimY == Vertex::Blue);

    MorphPosition::getOtherDims(Vertex::Red, dimX, dimY);
    REQUIRE(dimX == Vertex::Time);
    REQUIRE(dimY == Vertex::Blue);

    MorphPosition::getOtherDims(Vertex::Blue, dimX, dimY);
    REQUIRE(dimX == Vertex::Time);
    REQUIRE(dimY == Vertex::Red);
}
