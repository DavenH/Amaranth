#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Rasterization/RasterizerConversion.h"
#include "../src/Curve/Rasterization/GuideCurveOffsetSeeds.h"
#include "../src/Curve/Rasterization/Policies/RasterizerCleanupPolicy.h"
#include "../src/Curve/Rasterization/Policies/SnapshotPolicy.h"
#include "../src/Curve/Rasterization/RasterizationRequest.h"
#include "../src/Curve/Rasterization/RasterizerResult.h"
#include "../src/Curve/Rasterization/RasterizerRuntime.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

using namespace Rasterization;

namespace {
    class CountingRandom {
    public:
        int nextInt(int range) {
            int value = nextValue % range;
            ++nextValue;
            return value;
        }

        int nextValue {};
    };
}

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

TEST_CASE("GuideCurveOffsetSeeds owns paired phase and vertical seed arrays", "[rasterization][guide]") {
    GuideCurveOffsetSeeds seeds;
    CountingRandom random;

    seeds.randomize(3, 32, random);

    REQUIRE(seeds.vertical()[0] == 0);
    REQUIRE(seeds.phase()[0] == 1);
    REQUIRE(seeds.vertical()[1] == 2);
    REQUIRE(seeds.phase()[1] == 3);
    REQUIRE(seeds.vertical()[2] == 4);
    REQUIRE(seeds.phase()[2] == 5);
    REQUIRE(seeds.vertical()[3] == 0);
    REQUIRE(seeds.phase()[3] == 0);
    REQUIRE(seeds.verticalAt(2) == 4);
    REQUIRE(seeds.phaseAt(2) == 5);
    REQUIRE(seeds.verticalAt(GuideCurveOffsetSeeds::capacity) == 0);
    REQUIRE(seeds.phaseAt(-1) == 0);

    seeds.reset();

    REQUIRE(seeds.vertical()[0] == 0);
    REQUIRE(seeds.phase()[0] == 0);
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

    waveform.waveform.zeroIndex = 5;
    waveform.waveform.oneIndex = 9;
    waveform.sampleable = true;

    REQUIRE(intercepts.points.size() == 1);
    REQUIRE(intercepts.points.front().source == RasterPointSource::fxVertex(4));
    REQUIRE(intercepts.needsResort);

    REQUIRE(curves.frontPadding.size() == 1);
    REQUIRE(curves.backPadding.size() == 1);
    REQUIRE(curves.paddingSize == 1);

    REQUIRE(waveform.waveform.zeroIndex == 5);
    REQUIRE(waveform.waveform.oneIndex == 9);
    REQUIRE(waveform.sampleable);
}

TEST_CASE("WaveformBuffers groups waveform storage and reference assignment", "[rasterization][types][waveform]") {
    ScopedAlloc<float> sourceMemory;
    ScopedAlloc<float> targetMemory;

    WaveformBuffers source(1, 3);
    WaveformBuffers target(0, 0);
    source.place(sourceMemory, 4);
    target.place(targetMemory, 4);

    for (int i = 0; i < source.waveX.size(); ++i) {
        source.waveX[i] = float(i);
        source.waveY[i] = float(i) + 0.25f;
        source.diffX[i] = float(i) + 0.5f;
        source.slope[i] = float(i) + 0.75f;
        source.area[i] = float(i) + 1.f;
    }

    target.copyFrom(source);

    REQUIRE(target.zeroIndex == 1);
    REQUIRE(target.oneIndex == 3);
    REQUIRE(target.waveX[2] == source.waveX[2]);
    REQUIRE(target.waveY[2] == source.waveY[2]);
    REQUIRE(target.diffX[2] == source.diffX[2]);
    REQUIRE(target.slope[2] == source.slope[2]);
    REQUIRE(target.area[2] == source.area[2]);

    WaveformBuffers assigned(0, 0);
    WaveformBufferRefs refs(assigned);
    REQUIRE_FALSE(refs.isSampleable());

    refs.assignFrom(source);

    REQUIRE(assigned.zeroIndex == 1);
    REQUIRE(assigned.oneIndex == 3);
    REQUIRE(refs.isSampleable());
    REQUIRE(assigned.waveX.get() == source.waveX.get());
    REQUIRE(assigned.waveY.get() == source.waveY.get());
    REQUIRE(assigned.diffX.get() == source.diffX.get());
    REQUIRE(assigned.slope.get() == source.slope.get());
    REQUIRE(assigned.area.get() == source.area.get());

    WaveformBuffers copied(0, 0);
    copied.place(targetMemory, 4);
    WaveformBufferRefs copyRefs(copied);
    copyRefs.copyFrom(source);

    REQUIRE(copied.zeroIndex == 1);
    REQUIRE(copied.oneIndex == 3);
    REQUIRE(copied.waveX[2] == source.waveX[2]);
    REQUIRE(copied.waveY[2] == source.waveY[2]);
    REQUIRE(copied.diffX[2] == source.diffX[2]);
    REQUIRE(copied.slope[2] == source.slope[2]);
    REQUIRE(copied.area[2] == source.area[2]);
}

TEST_CASE("RasterizationRequest defaults match MeshRasterizer compatibility defaults", "[rasterization][request]") {
    RasterizationRequest request;

    REQUIRE(request.scalingMode == PointScalingMode::Unipolar);
    REQUIRE(request.cyclic);
    REQUIRE(request.calcDepthDimensions);
    REQUIRE_FALSE(request.calcInterceptsOnly);
    REQUIRE_FALSE(request.lowResCurves);
    REQUIRE_FALSE(request.integralSampling);
    REQUIRE(request.noiseSeed == -1);
    REQUIRE(request.overridingDimension == Vertex::Time);
    REQUIRE(request.primaryViewDimension == Vertex::Time);
    REQUIRE(request.paddingSize == 2);
    REQUIRE(request.xMinimum == 0.f);
    REQUIRE(request.xMaximum == 1.f);
}

TEST_CASE("MeshRasterizer exposes current state as RasterizationRequest", "[rasterization][request]") {
    MeshRasterizer rasterizer("RequestRasterizer");
    rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Red));
    rasterizer.setMorphPosition(MorphPosition(0.25f, 0.5f, 0.75f));
    rasterizer.setScalingMode(MeshRasterizer::HalfBipolar);
    rasterizer.setBatchMode(true);
    rasterizer.setWrapsEnds(false);
    rasterizer.setCalcDepthDimensions(false);
    rasterizer.setCalcInterceptsOnly(true);
    rasterizer.setIntegralSampling(true);
    rasterizer.setInterpolatesCurves(true);
    rasterizer.setLowresCurves(true);
    rasterizer.setDecoupleComponentDfrm(true);
    rasterizer.setNoiseSeed(17);
    rasterizer.setToOverrideDim(true);
    rasterizer.setOverridingDim(Vertex::Blue);
    rasterizer.setInterceptPadding(0.125f);
    rasterizer.setLimits(-0.5f, 1.5f);

    RasterizationRequest request = rasterizer.createRasterizationRequest();

    REQUIRE(request.dims.x == Vertex::Phase);
    REQUIRE(request.dims.y == Vertex::Amp);
    REQUIRE(request.dims.hidden.size() == 1);
    REQUIRE(request.dims.hidden.front() == Vertex::Red);
    REQUIRE(request.morph.time.getTargetValue() == 0.25f);
    REQUIRE(request.morph.red.getTargetValue() == 0.5f);
    REQUIRE(request.morph.blue.getTargetValue() == 0.75f);
    REQUIRE(request.scalingMode == PointScalingMode::HalfBipolar);
    REQUIRE(request.batchMode);
    REQUIRE_FALSE(request.cyclic);
    REQUIRE_FALSE(request.calcDepthDimensions);
    REQUIRE(request.calcInterceptsOnly);
    REQUIRE(request.integralSampling);
    REQUIRE(request.interpolateCurves);
    REQUIRE(request.lowResCurves);
    REQUIRE(request.decoupleComponentDeforms);
    REQUIRE_FALSE(request.publishSnapshot);
    REQUIRE(request.noiseSeed == 17);
    REQUIRE(request.overrideDimension);
    REQUIRE(request.overridingDimension == Vertex::Blue);
    REQUIRE(request.primaryViewDimension == Vertex::Blue);
    REQUIRE(request.interceptPadding == 0.125f);
    REQUIRE(request.xMinimum == -0.5f);
    REQUIRE(request.xMaximum == 1.5f);
}

TEST_CASE("MeshRasterizer primary view dimension can be supplied by a provider", "[rasterization][request]") {
    MeshRasterizer rasterizer("RequestRasterizer");
    rasterizer.setPrimaryViewDimensionProvider([]() { return Vertex::Red; });

    RasterizationRequest request = rasterizer.createRasterizationRequest();

    REQUIRE(request.primaryViewDimension == Vertex::Red);

    rasterizer.setToOverrideDim(true);
    rasterizer.setOverridingDim(Vertex::Blue);

    RasterizationRequest overrideRequest = rasterizer.createRasterizationRequest();

    REQUIRE(overrideRequest.primaryViewDimension == Vertex::Blue);
}

TEST_CASE("RasterizerRuntime defaults to an unbound compatibility view", "[rasterization][runtime]") {
    RasterizerRuntime runtime;

    REQUIRE_FALSE(runtime.isBound());
    REQUIRE_FALSE(runtime.isSampleable());
}

TEST_CASE("MeshRasterizer exposes mutable storage as RasterizerRuntime", "[rasterization][runtime]") {
    MeshRasterizer rasterizer("RuntimeRasterizer");
    RasterizerRuntime runtime = rasterizer.createRasterizerRuntime();

    REQUIRE(runtime.isBound());
    REQUIRE(runtime.intercepts != nullptr);
    REQUIRE(runtime.curves != nullptr);
    REQUIRE(runtime.frontPadding != nullptr);
    REQUIRE(runtime.backPadding != nullptr);
    REQUIRE(runtime.colorPoints == &rasterizer.getColorPoints());
    REQUIRE(runtime.waveform.waveX != nullptr);
    REQUIRE(runtime.waveform.waveY != nullptr);
    REQUIRE(runtime.waveform.diffX != nullptr);
    REQUIRE(runtime.waveform.slope != nullptr);
    REQUIRE(runtime.waveform.zeroIndex != nullptr);
    REQUIRE(runtime.waveform.oneIndex != nullptr);
    REQUIRE(runtime.paddingSize != nullptr);
    REQUIRE(runtime.unsampleable != nullptr);

    runtime.intercepts->emplace_back(0.25f, 0.75f);
    *runtime.paddingSize = 3;

    REQUIRE(runtime.intercepts->size() == 1);
    REQUIRE(runtime.interceptCount() == 1);
    REQUIRE(runtime.hasAtLeastIntercepts(1));
    REQUIRE_FALSE(runtime.hasAtLeastIntercepts(2));
    REQUIRE(rasterizer.getPaddingSize() == 3);

    runtime.clearIntercepts();

    REQUIRE(runtime.intercepts->empty());
}

TEST_CASE("RasterizerRuntime can expose a snapshot source view", "[rasterization][runtime][snapshot]") {
    MeshRasterizer rasterizer("RuntimeSnapshotRasterizer");
    RasterizerRuntime runtime = rasterizer.createRasterizerRuntime();

    runtime.intercepts->emplace_back(0.25f, 0.75f);
    runtime.curves->emplace_back(Intercept(0.f, 0.f), Intercept(0.5f, 0.5f), Intercept(1.f, 1.f));
    runtime.colorPoints->emplace_back(nullptr, Vertex2(), Vertex2(), Vertex2(), Vertex::Time);

    RasterizerSnapshotSource source = createRasterizerSnapshotSource(runtime);

    REQUIRE(source.intercepts == runtime.intercepts);
    REQUIRE(source.curves == runtime.curves);
    REQUIRE(source.colorPoints == runtime.colorPoints);
    REQUIRE(source.waveform.waveX.get() == runtime.waveform.waveX->get());
    REQUIRE(source.waveform.waveY.get() == runtime.waveform.waveY->get());
    REQUIRE(source.waveform.diffX.get() == runtime.waveform.diffX->get());
    REQUIRE(source.waveform.slope.get() == runtime.waveform.slope->get());
    REQUIRE(source.waveform.area.get() == runtime.waveform.area->get());
    REQUIRE(source.waveform.zeroIndex == *runtime.waveform.zeroIndex);
    REQUIRE(source.waveform.oneIndex == *runtime.waveform.oneIndex);
}

TEST_CASE("RasterizerCleanupPolicy clears selected runtime storage and marks waveform unsampleable", "[rasterization][runtime]") {
    MeshRasterizer rasterizer("CleanupRasterizer");
    RasterizerRuntime runtime = rasterizer.createRasterizerRuntime();

    runtime.intercepts->emplace_back(0.25f, 0.75f);
    runtime.curves->emplace_back(Intercept(0.f, 0.f), Intercept(0.5f, 0.5f), Intercept(1.f, 1.f));
    runtime.frontPadding->emplace_back(-1.f, 0.f);
    runtime.backPadding->emplace_back(2.f, 1.f);
    runtime.colorPoints->emplace_back(nullptr, Vertex2(), Vertex2(), Vertex2(), 0);
    *runtime.unsampleable = false;

    RasterizerCleanupPolicy().clean(runtime);

    REQUIRE(runtime.intercepts->empty());
    REQUIRE(runtime.curves->empty());
    REQUIRE(runtime.frontPadding->empty());
    REQUIRE(runtime.backPadding->empty());
    REQUIRE(runtime.colorPoints->empty());
    REQUIRE(*runtime.unsampleable);
    REQUIRE(runtime.waveform.waveX->empty());
    REQUIRE(runtime.waveform.waveY->empty());
}

TEST_CASE("RasterizerCleanupPolicy can preserve optional runtime storage", "[rasterization][runtime]") {
    MeshRasterizer rasterizer("PartialCleanupRasterizer");
    RasterizerRuntime runtime = rasterizer.createRasterizerRuntime();

    runtime.curves->emplace_back(Intercept(0.f, 0.f), Intercept(0.5f, 0.5f), Intercept(1.f, 1.f));
    runtime.frontPadding->emplace_back(-1.f, 0.f);
    runtime.backPadding->emplace_back(2.f, 1.f);
    runtime.colorPoints->emplace_back(nullptr, Vertex2(), Vertex2(), Vertex2(), 0);
    *runtime.unsampleable = false;

    RasterizerCleanupOptions options;
    options.clearCurves = false;
    options.clearFrontPadding = false;
    options.clearBackPadding = false;
    options.clearColorPoints = false;

    RasterizerCleanupPolicy(options).clean(runtime);

    REQUIRE_FALSE(runtime.curves->empty());
    REQUIRE_FALSE(runtime.frontPadding->empty());
    REQUIRE_FALSE(runtime.backPadding->empty());
    REQUIRE_FALSE(runtime.colorPoints->empty());
    REQUIRE(*runtime.unsampleable);
}
