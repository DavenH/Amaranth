#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/V2/Stages/V2InterpolatorStages.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendInterpolationCube(Mesh& mesh, float phaseOffset, float ampOffset) {
    auto* cube = new VertCube(&mesh);
    mesh.addCube(cube);

    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        bool timePole = false;
        bool redPole = false;
        bool bluePole = false;
        VertCube::getPoles(i, timePole, redPole, bluePole);

        auto* vert = cube->lineVerts[i];
        vert->values[Vertex::Time] = timePole ? 1.0f : 0.0f;
        vert->values[Vertex::Red] = redPole ? 1.0f : 0.0f;
        vert->values[Vertex::Blue] = bluePole ? 1.0f : 0.0f;

        vert->values[Vertex::Phase] = phaseOffset
            + 0.08f
            + (timePole ? 0.35f : 0.0f)
            + (redPole ? 0.23f : 0.0f)
            + (bluePole ? 0.17f : 0.0f);

        vert->values[Vertex::Amp] = ampOffset
            + 0.12f
            + (timePole ? 0.22f : 0.0f)
            + (redPole ? 0.15f : 0.0f)
            + (bluePole ? 0.18f : 0.0f);

        vert->values[Vertex::Curve] = 0.5f;
    }
}

void populateInterpolationMesh(Mesh& mesh) {
    appendInterpolationCube(mesh, -0.35f, 0.00f);
    appendInterpolationCube(mesh, 1.35f, 0.06f);
}

template <typename Stage>
void requireDeterministicStageOutput(
    Stage& stage,
    const V2InterpolatorContext& context,
    int expectedCount) {
    std::vector<Intercept> first;
    std::vector<Intercept> second;
    int firstCount = 0;
    int secondCount = 0;

    REQUIRE(stage.run(context, first, firstCount));
    REQUIRE(stage.run(context, second, secondCount));
    REQUIRE(firstCount == expectedCount);
    REQUIRE(secondCount == expectedCount);
    REQUIRE(first.size() == second.size());

    for (size_t i = 0; i < first.size(); ++i) {
        REQUIRE(first[i].x == Catch::Approx(second[i].x).margin(1e-7));
        REQUIRE(first[i].y == Catch::Approx(second[i].y).margin(1e-7));
    }
}
}

TEST_CASE("V2 mesh interpolation stages produce bounded deterministic intercepts", "[curve][v2][interp][mesh]") {
    ScopedMesh scoped("v2-mesh-interp");
    populateInterpolationMesh(scoped.mesh);

    V2InterpolatorContext context;
    context.mesh = &scoped.mesh;
    context.morph = MorphPosition(0.33f, 0.58f, 0.41f);
    context.wrapPhases = false;

    std::vector<Intercept> trilinear;
    std::vector<Intercept> bilinear;
    std::vector<Intercept> simple;
    int trilinearCount = 0;
    int bilinearCount = 0;
    int simpleCount = 0;

    V2TrilinearInterpolatorStage trilinearStage;
    V2BilinearInterpolatorStage bilinearStage;
    V2SimpleInterpolatorStage simpleStage;

    REQUIRE(trilinearStage.run(context, trilinear, trilinearCount));
    REQUIRE(bilinearStage.run(context, bilinear, bilinearCount));
    REQUIRE(simpleStage.run(context, simple, simpleCount));

    REQUIRE(trilinearCount == 2);
    REQUIRE(bilinearCount == 2);
    REQUIRE(simpleCount == 2);

    auto requireBounded = [](const std::vector<Intercept>& intercepts, int count) {
        REQUIRE(static_cast<int>(intercepts.size()) == count);
        for (int i = 0; i < count; ++i) {
            REQUIRE(intercepts[i].x > -0.5f);
            REQUIRE(intercepts[i].x < 2.0f);
            REQUIRE(intercepts[i].y > -0.5f);
            REQUIRE(intercepts[i].y < 2.0f);
        }
    };

    requireBounded(trilinear, trilinearCount);
    requireBounded(bilinear, bilinearCount);
    requireBounded(simple, simpleCount);

    requireDeterministicStageOutput(trilinearStage, context, 2);
    requireDeterministicStageOutput(bilinearStage, context, 2);
    requireDeterministicStageOutput(simpleStage, context, 2);
}

TEST_CASE("V2 mesh interpolation wrapPhases changes intercept domain when phases overflow", "[curve][v2][interp][mesh][wrap]") {
    ScopedMesh scoped("v2-mesh-wrap");
    populateInterpolationMesh(scoped.mesh);

    V2InterpolatorContext unwrappedContext;
    unwrappedContext.mesh = &scoped.mesh;
    unwrappedContext.morph = MorphPosition(0.45f, 0.55f, 0.62f);
    unwrappedContext.wrapPhases = false;

    V2InterpolatorContext wrappedContext = unwrappedContext;
    wrappedContext.wrapPhases = true;

    V2TrilinearInterpolatorStage stage;
    std::vector<Intercept> unwrapped;
    std::vector<Intercept> wrapped;
    int unwrappedCount = 0;
    int wrappedCount = 0;

    REQUIRE(stage.run(unwrappedContext, unwrapped, unwrappedCount));
    REQUIRE(stage.run(wrappedContext, wrapped, wrappedCount));
    REQUIRE(unwrappedCount == wrappedCount);
    REQUIRE(unwrappedCount == 2);

    bool wrappedInUnitDomain = true;
    bool unwrappedOutOfDomain = false;

    for (int i = 0; i < wrappedCount; ++i) {
        if (wrapped[i].x < 0.0f || wrapped[i].x >= 1.0f) {
            wrappedInUnitDomain = false;
        }

        if (unwrapped[i].x < 0.0f || unwrapped[i].x >= 1.0f) {
            unwrappedOutOfDomain = true;
        }
    }

    REQUIRE(wrappedInUnitDomain);
    REQUIRE(unwrappedOutOfDomain);
}
