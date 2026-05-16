#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Mesh/CollisionDetector.h"
#include "../src/Curve/Mesh/Mesh.h"
#include "../src/Curve/Mesh/VertCube.h"

namespace {
    struct MeshDeleter {
        void operator()(Mesh* mesh) const {
            if (mesh != nullptr) {
                mesh->destroy();
                delete mesh;
            }
        }
    };

    void setCubeAsLine(VertCube* cube, float lowPhase, float highPhase) {
        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            bool time, red, blue;
            VertCube::getPoles(i, time, red, blue);

            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Time]  = time ? 1.f : 0.f;
            vertex->values[Vertex::Red]   = red  ? 1.f : 0.f;
            vertex->values[Vertex::Blue]  = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp]   = 0.5f;
            vertex->values[Vertex::Curve] = 0.5f;
        }
    }
}

TEST_CASE("CollisionDetector refreshes moved selected vertices between validations", "[collision]") {
    std::unique_ptr<Mesh, MeshDeleter> mesh(new Mesh("CollisionRegression"));

    auto* staticCube = new VertCube(mesh.get());
    setCubeAsLine(staticCube, 0.25f, 0.75f);
    mesh->addCube(staticCube);

    auto* movingCube = new VertCube(mesh.get());
    setCubeAsLine(movingCube, 0.85f, 0.95f);
    mesh->addCube(movingCube);

    CollisionDetector detector(nullptr, CollisionDetector::Time);
    detector.setDimSlices(21);
    detector.setCurrentSelection(mesh.get(), movingCube);

    REQUIRE(detector.validate());

    setCubeAsLine(movingCube, 0.75f, 0.25f);

    REQUIRE_FALSE(detector.validate());
}
