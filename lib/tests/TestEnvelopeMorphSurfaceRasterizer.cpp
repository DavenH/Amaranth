#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <Array/ScopedAlloc.h>
#include <Curve/Curve.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>

namespace {
    struct CurveTableScope {
        CurveTableScope() : ownsTable(Curve::table == nullptr) {
            if (ownsTable) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (ownsTable) {
                Curve::deleteTable();
            }
        }

        bool ownsTable;
    };

    struct MorphSurfaceMesh {
        MorphSurfaceMesh() : mesh("EnvelopeMorphSurface") {
            constexpr float phases[] { 0.2f, 0.5f, 0.8f };
            for (float phase : phases) {
                auto* cube = new VertCube(&mesh);
                for (int i = 0; i < (int) VertCube::numVerts; ++i) {
                    bool time, red, blue;
                    VertCube::getPoles(i, time, red, blue);
                    Vertex* vertex = cube->getVertex(i);
                    vertex->values[Vertex::Time] = time ? 1.f : 0.f;
                    vertex->values[Vertex::Red] = red ? 1.f : 0.f;
                    vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
                    vertex->values[Vertex::Phase] = phase;
                    vertex->values[Vertex::Amp] = red ? 0.8f : 0.2f;
                    vertex->values[Vertex::Curve] = 0.5f;
                }
                mesh.addCube(cube);
            }
        }

        ~MorphSurfaceMesh() {
            mesh.destroy();
        }

        Mesh mesh;
    };
}

TEST_CASE("Envelope morph surface rasterizer preserves E3 request semantics", "[rasterization][envelope-surface]") {
    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    const auto& request = rasterizer.getRequest();

    REQUIRE(request.lowResCurves);
    REQUIRE_FALSE(request.cyclic);
    REQUIRE_FALSE(request.calcDepthDimensions);
    REQUIRE(request.scalingMode == Rasterization::PointScalingMode::HalfBipolar);
    REQUIRE(request.xMinimum == 0.f);
    REQUIRE(request.xMaximum == 10.f);
}

TEST_CASE("Envelope morph surface rasterizer clears columns for an absent mesh", "[rasterization][envelope-surface]") {
    constexpr int columnSize = 8;
    ScopedAlloc<Float32> memory(columnSize * 2);
    Buffer<float> first = memory.place(columnSize);
    Buffer<float> second = memory.place(columnSize);
    first.set(1.f);
    second.set(1.f);

    std::vector<Column> columns { Column(first), Column(second) };
    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    rasterizer.renderSurface(nullptr, columns, columnSize, Vertex::Time, MorphPosition());

    REQUIRE(columns[0].sum() == 0.f);
    REQUIRE(columns[1].sum() == 0.f);
}

TEST_CASE("Envelope morph surface renders each explicit morph cross-section and restores context",
        "[rasterization][envelope-surface]") {
    CurveTableScope curveTable;
    MorphSurfaceMesh surfaceMesh;
    constexpr int columnSize = 16;
    ScopedAlloc<Float32> memory(columnSize * 3);
    std::vector<Column> columns {
        Column(memory.place(columnSize)),
        Column(memory.place(columnSize)),
        Column(memory.place(columnSize))
    };

    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    MorphPosition baseMorph(0.5f, 0.37f, 0.5f);
    rasterizer.renderSurface(
            &surfaceMesh.mesh,
            columns,
            columnSize,
            Vertex::Red,
            baseMorph);

    REQUIRE(columns[0].sum() < columns[1].sum());
    REQUIRE(columns[1].sum() < columns[2].sum());
    REQUIRE(rasterizer.getMorphPosition().time.getCurrentValue()
            == Catch::Approx(baseMorph.time.getCurrentValue()));
    REQUIRE(rasterizer.getMorphPosition().red.getCurrentValue()
            == Catch::Approx(baseMorph.red.getCurrentValue()));
    REQUIRE(rasterizer.getMorphPosition().blue.getCurrentValue()
            == Catch::Approx(baseMorph.blue.getCurrentValue()));
}
