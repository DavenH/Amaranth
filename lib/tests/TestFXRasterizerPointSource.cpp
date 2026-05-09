#include <array>
#include <memory>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/FXRasterizer.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/Rasterization/Adapters/FxRasterizerAdapter.h"
#include "../src/Curve/Rasterization/RasterizerComposer.h"
#include "../src/Curve/Rasterization/Sources/VertexListSource.h"
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

    RasterizerCompare::Snapshot captureFxPipelineOutput(
            const Rasterization::FxRasterizationPipeline::Output& output) {
        RasterizerCompare::Snapshot snapshot;

        snapshot.waveX      = copyBuffer(output.waveform.waveX);
        snapshot.waveY      = copyBuffer(output.waveform.waveY);
        snapshot.diffX      = copyBuffer(output.waveform.diffX);
        snapshot.slope      = copyBuffer(output.waveform.slope);
        snapshot.curves     = output.curves;
        snapshot.intercepts = output.intercepts;
        snapshot.zeroIndex  = output.waveform.zeroIndex;
        snapshot.oneIndex   = output.waveform.oneIndex;
        snapshot.sampleable = output.sampleable;

        return snapshot;
    }
}

TEST_CASE("VertexListSource exposes FX vertices without a Mesh", "[rasterization][fx]") {
    std::vector<std::unique_ptr<Vertex>> ownedVertices;
    ownedVertices.emplace_back(makeOwnedVertex(0.25f, 0.75f, 0.50f));

    std::vector<Vertex*> vertices { ownedVertices.front().get() };
    Rasterization::VertexListSource source(&vertices);

    REQUIRE_FALSE(source.empty());
    REQUIRE(source.size() == 1);

    Rasterization::RasterPoint point = source.pointAt(0);

    REQUIRE(point.x == Catch::Approx(0.25f));
    REQUIRE(point.y == Catch::Approx(0.75f));
    REQUIRE(point.sharpness == Catch::Approx(0.50f));
    REQUIRE(point.adjustedX == Catch::Approx(0.25f));
    REQUIRE(point.source == Rasterization::RasterPointSource::fxVertex(0));
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

    REQUIRE(rasterizer.getMesh() == nullptr);
    REQUIRE(rasterizer.getNumDims() == 1);
    REQUIRE(rasterizer.hasEnoughCubesForCrossSection());
    REQUIRE(rasterizer.isSampleable());
    REQUIRE(rasterizer.getWaveX().size() > 0);
    REQUIRE(rasterizer.getWaveY().size() == rasterizer.getWaveX().size());

    std::array<float, 32> samples {};
    Buffer<float> sampleBuffer(samples.data(), (int) samples.size());
    rasterizer.sampleEvenlyTo(sampleBuffer);

    REQUIRE(copyBuffer(sampleBuffer).front() == Catch::Approx(rasterizer.sampleAt(0.0)));
}

TEST_CASE("FXRasterizer mesh adapter matches direct vertex list rasterization", "[rasterization][fx]") {
    CurveTableScope curveTableScope;
    MeshOwner owner("FxPointSourceMesh");
    populateFxMesh(owner.mesh);
    std::vector<Vertex*> directVertices = owner.mesh.getVerts();

    FXRasterizer meshRasterizer(nullptr, "FxMeshAdapter");
    meshRasterizer.setMesh(&owner.mesh);
    meshRasterizer.calcCrossPoints();
    meshRasterizer.makeCopy();

    FXRasterizer directRasterizer(nullptr, "FxDirectVertices");
    directRasterizer.setVertices(&directVertices);
    directRasterizer.calcCrossPoints();
    directRasterizer.makeCopy();

    RasterizerCompare::requireSnapshotNear(
            RasterizerCompare::capture(directRasterizer),
            RasterizerCompare::capture(meshRasterizer));

    std::array<float, 64> meshSamples {};
    std::array<float, 64> directSamples {};

    Buffer<float> meshBuffer(meshSamples.data(), (int) meshSamples.size());
    Buffer<float> directBuffer(directSamples.data(), (int) directSamples.size());

    meshRasterizer.sampleEvenlyTo(meshBuffer);
    directRasterizer.sampleEvenlyTo(directBuffer);

    RasterizerCompare::requireBufferNear(copyBuffer(directBuffer), copyBuffer(meshBuffer));
}

TEST_CASE("RasterizerComposer builds an FX rasterizer without a Mesh", "[rasterization][fx][composer]") {
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

    Rasterization::RasterizationRequest request;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;

    auto rasterizer = Rasterization::RasterizerComposer::fx()
            .withVertices(&vertices)
            .withRequest(request)
            .build();

    const auto& output = rasterizer.render();

    REQUIRE(rasterizer.getSource().size() == 4);
    REQUIRE_FALSE(rasterizer.getRequest().cyclic);
    REQUIRE_FALSE(rasterizer.getRequest().calcDepthDimensions);
    REQUIRE(output.sampleable);
    REQUIRE(output.intercepts.front().y == Catch::Approx(-0.6f));
    REQUIRE_FALSE(output.waveform.waveX.empty());
    REQUIRE(output.waveform.waveY.size() == output.waveform.waveX.size());
}

TEST_CASE("FxRasterizerAdapter publishes composed FX output through runtime", "[rasterization][fx][composer]") {
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

    MeshRasterizer storage("FxAdapterStorage");
    Rasterization::RasterizationRequest request = storage.createRasterizationRequest();
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::PointScalingMode::Bipolar;

    Rasterization::FxRasterizerAdapter adapter;
    adapter.setVertices(&vertices);

    REQUIRE(adapter.hasEnoughCubesForCrossSection());
    REQUIRE(adapter.render(request, storage.createRasterizerRuntime()));
    REQUIRE(storage.isSampleable());
    REQUIRE(storage.getCurves().size() > 0);
    REQUIRE(storage.getWaveX().size() > 0);
    REQUIRE(storage.getWaveY().size() == storage.getWaveX().size());

    adapter.setVertices(nullptr);
    REQUIRE_FALSE(adapter.render(request, storage.createRasterizerRuntime()));
    REQUIRE(storage.wasCleanedUp());
    REQUIRE(storage.getWaveX().empty());
}

TEST_CASE("FXRasterizer is a compatibility adapter over the FX composer", "[rasterization][fx][composer]") {
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

    FXRasterizer adapter(nullptr, "FxComposerAdapter");
    adapter.setVertices(&vertices);
    adapter.calcCrossPoints();
    adapter.makeCopy();

    Rasterization::RasterizationRequest request = adapter.createRasterizationRequest();
    request.cyclic = false;
    request.calcDepthDimensions = false;
    request.scalingMode = Rasterization::pointScalingModeFromLegacyFx(MeshRasterizer::Unipolar);

    auto composed = Rasterization::RasterizerComposer::fx()
            .withVertices(&vertices)
            .withRequest(request)
            .build();

    const auto& output = composed.render();

    RasterizerCompare::requireSnapshotNear(
            RasterizerCompare::capture(adapter),
            captureFxPipelineOutput(output));
}
