#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Curve.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/VertCube.h"

namespace {
    struct MeshDeleter {
        void operator()(Mesh* mesh) const {
            if (mesh != nullptr) {
                mesh->destroy();
                delete mesh;
            }
        }
    };

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

    std::unique_ptr<Mesh, MeshDeleter> createSyntheticWaveMesh() {
        std::unique_ptr<Mesh, MeshDeleter> mesh(new Mesh("SyntheticWave"));

        struct CubeData {
            float lowPhase, highPhase;
            float lowAmp, highAmp;
            float sharpness;
        };

        const CubeData cubes[] = {
            { 0.03f, 0.05f, 0.22f, 0.28f, 0.20f },
            { 0.31f, 0.35f, 0.68f, 0.62f, 1.00f },
            { 0.57f, 0.61f, 0.18f, 0.24f, 0.45f },
            { 0.93f, 0.97f, 0.81f, 0.76f, 0.25f },
        };

        for (const auto& cubeData : cubes) {
            auto* cube = new VertCube(mesh.get());
            setCubeAsConstantPoint(cube,
                                   cubeData.lowPhase,
                                   cubeData.highPhase,
                                   cubeData.lowAmp,
                                   cubeData.highAmp,
                                   cubeData.sharpness);
            mesh->addCube(cube);
        }

        return mesh;
    }

    void requireFinite(Buffer<float> buffer) {
        for (int i = 0; i < buffer.size(); ++i) {
            INFO("index=" << i << " value=" << buffer[i]);
            REQUIRE(std::isfinite(buffer[i]));
        }
    }

    void requireNonDecreasing(Buffer<float> buffer, float tolerance = 2e-3f) {
        for (int i = 1; i < buffer.size(); ++i) {
            INFO("index=" << i
                 << " previous=" << buffer[i - 1]
                 << " current=" << buffer[i]
                 << " tolerance=" << tolerance);
            REQUIRE(buffer[i] + tolerance >= buffer[i - 1]);
        }
    }
}

TEST_CASE("MeshRasterizer creates finite monotonic wave buffers for a synthetic mesh", "[meshrasterizer][wave]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    auto waveX = rasterizer.getWaveX();
    auto waveY = rasterizer.getWaveY();

    REQUIRE_FALSE(waveX.empty());
    REQUIRE(waveX.size() == waveY.size());
    REQUIRE(rasterizer.isSampleable());

    requireFinite(waveX);
    requireFinite(waveY);
    requireNonDecreasing(waveX);

    REQUIRE(waveX.front() < 0.f);
    REQUIRE(waveX.back() > 1.f);
    REQUIRE(rasterizer.getZeroIndex() >= 0);
    REQUIRE(rasterizer.getZeroIndex() < waveX.size() - 1);
    REQUIRE(rasterizer.getOneIndex() >= 0);
    REQUIRE(rasterizer.getOneIndex() < waveX.size() - 1);

}

TEST_CASE("MeshRasterizer can sample evenly after max-sharpness waveform generation", "[meshrasterizer][wave][sampling]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    ScopedAlloc<Float32> sampleMemory;
    sampleMemory.ensureSize(128);
    Buffer<float> samples = sampleMemory.place(128);
    rasterizer.sampleEvenlyTo(samples);

    requireFinite(samples);

}
