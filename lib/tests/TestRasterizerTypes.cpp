#include <catch2/catch_test_macros.hpp>
#include <type_traits>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Rasterization/GuideCurveOffsetSeeds.h"
#include "../src/Curve/Rasterization/RasterizationRequest.h"
#include "Support/LegacyMeshRasterizer.h"
#include "../src/Curve/Mesh/Mesh.h"
#include "../src/Curve/Mesh/VertCube.h"

using namespace Rasterization;

static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<GeometryRenderCommand>().request)>>);
static_assert(std::is_const_v<std::remove_reference_t<
        decltype(std::declval<WaveformRenderCommand>().request)>>);

TEST_CASE("GuideCurveOffsetSeeds owns paired phase and vertical seed arrays", "[rasterization][guide]") {
    GuideCurveOffsetSeeds seeds;

    seeds.derive(3, 32, GuideCurveSeed::visualization(17u));

    REQUIRE(seeds.vertical()[0] >= 0);
    REQUIRE(seeds.vertical()[0] < 32);
    REQUIRE(seeds.phase()[0] >= 0);
    REQUIRE(seeds.phase()[0] < 32);
    REQUIRE(seeds.verticalAt(2) >= 0);
    REQUIRE(seeds.phaseAt(2) >= 0);
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

TEST_CASE("GuideCurveOffsetSeeds derive reproducibly from semantic identity",
        "[rasterization][guide][seed]") {
    GuideCurveOffsetSeeds first;
    GuideCurveOffsetSeeds repeated;
    GuideCurveOffsetSeeds unrelated;
    GuideCurveOffsetSeeds reordered;
    const auto layerSeed = GuideCurveSeed::visualization(0x4c415945u);

    first.derive(4, 256, layerSeed);
    unrelated.derive(4, 256, GuideCurveSeed::visualization(0x4f544852u));
    reordered.derive(4, 256, GuideCurveSeed::visualization(0x4f544852u));
    repeated.derive(4, 256, layerSeed);

    for (int i = 0; i < 4; ++i) {
        REQUIRE(first.phaseAt(i) == repeated.phaseAt(i));
        REQUIRE(first.verticalAt(i) == repeated.verticalAt(i));
    }

    REQUIRE(first.phaseAt(0) != unrelated.phaseAt(0));
    REQUIRE(first.verticalAt(0) != unrelated.verticalAt(0));
    REQUIRE(unrelated.phaseAt(0) == reordered.phaseAt(0));
    REQUIRE(unrelated.verticalAt(0) == reordered.verticalAt(0));

    GuideCurveOffsetSeeds firstVoice;
    GuideCurveOffsetSeeds secondVoice;
    firstVoice.derive(2, 256, GuideCurveSeed::voiceLifecycle(101u));
    secondVoice.derive(2, 256, GuideCurveSeed::voiceLifecycle(202u));
    REQUIRE(firstVoice.phaseAt(0) != secondVoice.phaseAt(0));
    REQUIRE(firstVoice.verticalAt(0) != secondVoice.verticalAt(0));
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
    REQUIRE_FALSE(request.lowResCurves);
    REQUIRE_FALSE(request.integralSampling);
    REQUIRE(request.noiseSeed == -1);
    REQUIRE(request.overridingDimension == Vertex::Time);
    REQUIRE(request.primaryViewDimension == Vertex::Time);
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
    REQUIRE_FALSE(request.cyclic);
    REQUIRE_FALSE(request.calcDepthDimensions);
    REQUIRE(request.integralSampling);
    REQUIRE(request.interpolateCurves);
    REQUIRE(request.lowResCurves);
    REQUIRE(request.decoupleComponentDeforms);
    REQUIRE(request.noiseSeed == 17);
    REQUIRE(request.overrideDimension);
    REQUIRE(request.overridingDimension == Vertex::Blue);
    REQUIRE(request.primaryViewDimension == Vertex::Blue);
    REQUIRE(request.interceptPadding == 0.125f);
    REQUIRE(request.xMinimum == -0.5f);
    REQUIRE(request.xMaximum == 1.5f);
}
