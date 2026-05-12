#include <catch2/catch_test_macros.hpp>

#include <Curve/VertCube.h>
#include <UI/Panels/Panel.h>

TEST_CASE("Panel line paths use phase guide assignments for time rails", "[panel][guide]") {
    VertCube cube;
    cube.guideCurveAt(Vertex::Time) = 2;
    cube.guideCurveAt(Vertex::Phase) = 5;
    cube.guideCurveAt(Vertex::Red) = 7;
    cube.guideCurveAt(Vertex::Blue) = 11;

    REQUIRE(Panel::getLinePathPhaseGuideDimension(Vertex::Time) == Vertex::Phase);
    REQUIRE(Panel::getLinePathPhaseGuideChannel(cube, Vertex::Time) == 5);
    REQUIRE(Panel::getLinePathPhaseGuideChannel(cube, Vertex::Time) != 2);

    REQUIRE(Panel::getLinePathPhaseGuideDimension(Vertex::Red) == Vertex::Red);
    REQUIRE(Panel::getLinePathPhaseGuideChannel(cube, Vertex::Red) == 7);

    REQUIRE(Panel::getLinePathPhaseGuideDimension(Vertex::Blue) == Vertex::Blue);
    REQUIRE(Panel::getLinePathPhaseGuideChannel(cube, Vertex::Blue) == 11);
}
