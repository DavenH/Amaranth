#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Rasterization/RasterizerConversion.h"
#include "../src/Curve/Rasterization/RasterizerResult.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

using namespace Rasterization;

TEST_CASE("RasterPoint converts legacy Intercept fields losslessly", "[rasterization][types]") {
    Intercept intercept(0.25f, 0.75f, nullptr, 0.5f);
    intercept.adjustedX = 0.35f;
    intercept.padBefore = true;
    intercept.padAfter = true;
    intercept.isWrapped = true;

    RasterPoint point = toRasterPoint(intercept, RasterPointSource::externalPoint(12));
    Intercept roundTrip = toIntercept(point);

    REQUIRE(point.x == intercept.x);
    REQUIRE(point.y == intercept.y);
    REQUIRE(point.sharpness == intercept.shp);
    REQUIRE(point.adjustedX == intercept.adjustedX);
    REQUIRE(point.padBefore == intercept.padBefore);
    REQUIRE(point.padAfter == intercept.padAfter);
    REQUIRE(point.isWrapped == intercept.isWrapped);
    REQUIRE(point.source == RasterPointSource::externalPoint(12));

    REQUIRE(roundTrip.x == intercept.x);
    REQUIRE(roundTrip.y == intercept.y);
    REQUIRE(roundTrip.shp == intercept.shp);
    REQUIRE(roundTrip.adjustedX == intercept.adjustedX);
    REQUIRE(roundTrip.padBefore == intercept.padBefore);
    REQUIRE(roundTrip.padAfter == intercept.padAfter);
    REQUIRE(roundTrip.isWrapped == intercept.isWrapped);
    REQUIRE(roundTrip.cube == nullptr);
}

TEST_CASE("RasterPoint source metadata distinguishes mesh, FX, and external points", "[rasterization][types]") {
    Mesh mesh("RasterPointSourceMesh");
    auto* cube = new VertCube(&mesh);
    mesh.addCube(cube);

    Intercept meshIntercept(0.1f, 0.2f, cube, 0.3f);
    RasterPoint meshPoint = toRasterPoint(meshIntercept);
    MeshPointSourceRef meshRef = meshSourceRefFor(meshIntercept);

    RasterPoint fxPoint;
    fxPoint.source = RasterPointSource::fxVertex(7);

    RasterPoint externalPoint;
    externalPoint.source = RasterPointSource::externalPoint(3);

    REQUIRE(meshPoint.source.type == RasterPointSource::Type::MeshCube);
    REQUIRE(meshRef.cube == meshIntercept.cube);
    REQUIRE(fxPoint.source == RasterPointSource::fxVertex(7));
    REQUIRE(externalPoint.source == RasterPointSource::externalPoint(3));
    REQUIRE(fxPoint.source != externalPoint.source);

    mesh.destroy();
}

TEST_CASE("RasterizerTypes remains lightweight for point-list users", "[rasterization][types]") {
    REQUIRE(std::is_trivially_copyable<RasterPointSource>::value);
    REQUIRE(std::is_trivially_copyable<RasterPoint>::value);
    REQUIRE(std::is_trivially_copyable<MeshPointSourceRef>::value);
}

TEST_CASE("Rasterizer result shapes expose stage outputs without behavior", "[rasterization][types]") {
    InterceptBuildResult intercepts;
    CurveBuildResult curves;
    WaveformResult waveform;

    intercepts.points.push_back({ 0.1f, 0.2f, 0.3f, 0.4f, true, false, true, RasterPointSource::fxVertex(4) });
    intercepts.needsResort = true;

    curves.frontPadding.emplace_back(-1.f, 0.2f);
    curves.backPadding.emplace_back(2.f, 0.8f);
    curves.paddingSize = 1;

    waveform.zeroIndex = 5;
    waveform.oneIndex = 9;
    waveform.sampleable = true;

    REQUIRE(intercepts.points.size() == 1);
    REQUIRE(intercepts.points.front().source == RasterPointSource::fxVertex(4));
    REQUIRE(intercepts.needsResort);

    REQUIRE(curves.frontPadding.size() == 1);
    REQUIRE(curves.backPadding.size() == 1);
    REQUIRE(curves.paddingSize == 1);

    REQUIRE(waveform.zeroIndex == 5);
    REQUIRE(waveform.oneIndex == 9);
    REQUIRE(waveform.sampleable);
}
