#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Mesh.h"
#include "../src/Curve/Rasterization/Policies/Mesh/DepthProjectionPolicy.h"
#include "../src/Curve/VertCube.h"

using namespace Rasterization;

namespace {
    void setCubeAsConstantPoint(
            VertCube* cube,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            bool time, red, blue;
            VertCube::getPoles(i, time, red, blue);

            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Time]  = time ? 1.f : 0.f;
            vertex->values[Vertex::Red]   = red  ? 1.f : 0.f;
            vertex->values[Vertex::Blue]  = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp]   = time ? highAmp   : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    struct DepthFixture {
        DepthFixture() :
                cube(new VertCube(&mesh)) {
            mesh.addCube(cube);
            setCubeAsConstantPoint(cube, 0.25f, 0.75f, 0.2f, 0.8f, 0.5f);

            context.enabled          = true;
            context.cyclic           = true;
            context.visibleDimension = Vertex::Time;
            context.sliceDimension   = Vertex::Time;
            context.independent      = 0.5f;
            context.oscPhase         = 0.f;
            context.dims             = Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue);
            context.morph            = MorphPosition(0.5f, 0.5f, 0.5f);

            cube->getInterceptsFast(context.sliceDimension, reduction, context.morph);
            VertCube::vertexAt(
                    context.independent,
                    context.sliceDimension,
                    &reduction.v0,
                    &reduction.v1,
                    &reduction.v);

            midIntercept = Intercept(
                    reduction.v.values[Vertex::Phase] + context.oscPhase,
                    reduction.v.values[Vertex::Amp],
                    cube);
            midIntercept.adjustedX = midIntercept.x;
        }

        ~DepthFixture() {
            mesh.destroy();
        }

        Mesh mesh { "DepthProjectionPolicyMesh" };
        VertCube* cube {};
        VertCube::ReductionData reduction;
        DepthProjectionPolicy::Context context;
        Intercept midIntercept;
    };

    void applyNoGuide(Intercept&, const MorphPosition&, bool) {
    }
}

TEST_CASE("DepthProjectionPolicy emits hidden-dimension color points", "[rasterization][depth]") {
    DepthFixture fixture;
    std::vector<ColorPoint> colorPoints;

    DepthProjectionPolicy(true).project(
            fixture.context,
            fixture.cube,
            fixture.reduction,
            fixture.midIntercept,
            applyNoGuide,
            colorPoints);

    REQUIRE(colorPoints.size() == 3);

    int timePoints = 0;
    int redPoints = 0;
    int bluePoints = 0;

    for (const auto& colorPoint : colorPoints) {
        REQUIRE(colorPoint.cube == fixture.cube);

        if (colorPoint.num == Vertex::Time) {
            ++timePoints;
        } else if (colorPoint.num == Vertex::Red) {
            ++redPoints;
        } else if (colorPoint.num == Vertex::Blue) {
            ++bluePoints;
        } else {
            FAIL("unexpected hidden dimension");
        }
    }

    REQUIRE(timePoints == 1);
    REQUIRE(redPoints == 1);
    REQUIRE(bluePoints == 1);
}

TEST_CASE("DepthProjectionPolicy can be disabled explicitly", "[rasterization][depth]") {
    DepthFixture fixture;
    std::vector<ColorPoint> colorPoints;

    DepthProjectionPolicy(false).project(
            fixture.context,
            fixture.cube,
            fixture.reduction,
            fixture.midIntercept,
            applyNoGuide,
            colorPoints);

    REQUIRE(colorPoints.empty());

    DepthProjectionPolicy(true).project(
            DepthProjectionPolicy::Context(),
            fixture.cube,
            fixture.reduction,
            fixture.midIntercept,
            applyNoGuide,
            colorPoints);

    REQUIRE(colorPoints.empty());
}
