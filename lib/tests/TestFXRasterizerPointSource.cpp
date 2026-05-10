#include <array>
#include <memory>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/FXRasterizer.h"
#include "../src/Curve/Mesh.h"
#include "Support/LegacyMeshRasterizer.h"
#include "RasterizerCompare.h"

namespace {
    struct CurveTableScope {
        CurveTableScope() {
            if (refCount++ == 0) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (--refCount == 0) {
                Curve::deleteTable();
            }
        }

        inline static int refCount = 0;
    };

    struct MeshOwner {
        explicit MeshOwner(const String& name) :
                mesh(name) {
        }

        ~MeshOwner() {
            mesh.destroy();
        }

        Mesh mesh;
    };

    std::unique_ptr<Vertex> makeOwnedVertex(float phase, float amp, float curve) {
        auto vertex = std::make_unique<Vertex>();
        vertex->values[Vertex::Phase] = phase;
        vertex->values[Vertex::Amp] = amp;
        vertex->values[Vertex::Curve] = curve;

        return vertex;
    }

    Vertex* addVertex(Mesh& mesh, float phase, float amp, float curve) {
        auto* vertex = new Vertex();
        vertex->values[Vertex::Phase] = phase;
        vertex->values[Vertex::Amp] = amp;
        vertex->values[Vertex::Curve] = curve;
        mesh.addVertex(vertex);

        return vertex;
    }

    void populateFxMesh(Mesh& mesh) {
        addVertex(mesh, 0.08f, 0.20f, 0.15f);
        addVertex(mesh, 0.35f, 0.82f, 0.40f);
        addVertex(mesh, 0.72f, 0.38f, 0.65f);
        addVertex(mesh, 0.94f, 0.60f, 0.25f);
    }

    std::vector<float> copyBuffer(Buffer<float> buffer) {
        std::vector<float> values;
        values.reserve(buffer.size());

        for (int i = 0; i < buffer.size(); ++i) {
            values.emplace_back(buffer[i]);
        }

        return values;
    }

    void sampleEvenly(FXRasterizer& rasterizer, Buffer<float> dest) {
        rasterizer.samplerView().sampleWithInterval(dest, 1.f / float(dest.size() - 1), 0.f);
    }

}

TEST_CASE("FXRasterizer can rasterize a direct vertex list", "[rasterization][fx]") {
    CurveTableScope curveTableScope;
    std::vector<std::unique_ptr<Vertex>> ownedVertices;
    std::vector<Vertex*> vertices;

    ownedVertices.emplace_back(makeOwnedVertex(0.08f, 0.20f, 0.15f));
    ownedVertices.emplace_back(makeOwnedVertex(0.35f, 0.82f, 0.40f));
    ownedVertices.emplace_back(makeOwnedVertex(0.72f, 0.38f, 0.65f));
    ownedVertices.emplace_back(makeOwnedVertex(0.94f, 0.60f, 0.25f));

    for (auto& vertex : ownedVertices) {
        vertices.emplace_back(vertex.get());
    }

    FXRasterizer rasterizer(nullptr, "DirectFxVertexList");
    rasterizer.setVertices(&vertices);
    rasterizer.calcCrossPoints();

    REQUIRE(rasterizer.canRasterizeWaveform());
    REQUIRE(rasterizer.samplerView().isSampleable());

    std::array<float, 32> samples {};
    Buffer<float> sampleBuffer(samples.data(), (int) samples.size());
    sampleEvenly(rasterizer, sampleBuffer);

    REQUIRE(copyBuffer(sampleBuffer).front() == Catch::Approx(rasterizer.samplerView().sampleAt(0.0)));
}

TEST_CASE("FXRasterizer mesh adapter matches direct vertex list rasterization", "[rasterization][fx]") {
    CurveTableScope curveTableScope;
    MeshOwner owner("FxPointSourceMesh");
    populateFxMesh(owner.mesh);
    std::vector<Vertex*> directVertices = owner.mesh.getVerts();

    FXRasterizer meshRasterizer(nullptr, "FxMeshAdapter");
    meshRasterizer.setMesh(&owner.mesh);
    meshRasterizer.calcCrossPoints();

    FXRasterizer directRasterizer(nullptr, "FxDirectVertices");
    directRasterizer.setVertices(&directVertices);
    directRasterizer.calcCrossPoints();

    const auto& directData = directRasterizer.snapshotView().rasterizerData();
    const auto& meshData = meshRasterizer.snapshotView().rasterizerData();

    REQUIRE(directData.intercepts.size() == meshData.intercepts.size());
    for (int i = 0; i < (int) directData.intercepts.size(); ++i) {
        INFO("intercept=" << i);
        RasterizerCompare::requireInterceptNear(directData.intercepts[i], meshData.intercepts[i]);
    }

    REQUIRE(directData.curves.size() == meshData.curves.size());
    for (int i = 0; i < (int) directData.curves.size(); ++i) {
        INFO("curve=" << i);
        RasterizerCompare::requireCurveNear(directData.curves[i], meshData.curves[i]);
    }

    REQUIRE(directData.colorPoints.size() == meshData.colorPoints.size());
    for (int i = 0; i < (int) directData.colorPoints.size(); ++i) {
        INFO("colorPoint=" << i);
        RasterizerCompare::requireColorPointNear(directData.colorPoints[i], meshData.colorPoints[i]);
    }

    std::array<float, 64> meshSamples {};
    std::array<float, 64> directSamples {};

    Buffer<float> meshBuffer(meshSamples.data(), (int) meshSamples.size());
    Buffer<float> directBuffer(directSamples.data(), (int) directSamples.size());

    sampleEvenly(meshRasterizer, meshBuffer);
    sampleEvenly(directRasterizer, directBuffer);

    RasterizerCompare::requireBufferNear(copyBuffer(directBuffer), copyBuffer(meshBuffer));
}
