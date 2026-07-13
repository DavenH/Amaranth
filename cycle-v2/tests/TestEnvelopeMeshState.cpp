#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>

#include "../src/Nodes/Envelope/EnvelopeMeshState.h"

using namespace CycleV2;

namespace {

VertCube* addEnvelopeCube(EnvelopeMesh& mesh, float x, float lowAmp, float highAmp) {
    auto* cube = new VertCube(&mesh);
    MorphPosition position;
    position.time.setValueDirect(0.f);
    position.timeDepth = 0.001f;
    position.red.setValueDirect(0.f);
    position.redDepth = 1.f;
    position.blue.setValueDirect(0.f);
    position.blueDepth = 1.f;
    cube->initVerts(position);

    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        Vertex* vertex = cube->getVertex(i);
        vertex->values[Vertex::Phase] = x;
        vertex->values[Vertex::Amp] = vertex->values[Vertex::Red] > 0.5f
                ? highAmp
                : lowAmp;
        vertex->values[Vertex::Curve] = 0.4f;
    }

    mesh.addCube(cube);
    return cube;
}

}

TEST_CASE("Envelope mesh snapshots preserve morph vertices and lifecycle markers", "[cycle-v2][envelope]") {
    EnvelopeMesh source("SourceEnvelope");
    VertCube* first = addEnvelopeCube(source, 0.f, 0.1f, 0.8f);
    VertCube* second = addEnvelopeCube(source, 0.75f, 0.4f, 0.9f);
    source.loopCubes.insert(first);
    source.sustainCubes.insert(second);

    const String serialized = EnvelopeMeshState::serialize(source);
    EnvelopeMesh restored("RestoredEnvelope");

    REQUIRE(EnvelopeMeshState::apply(serialized, restored));
    REQUIRE(restored.getCubes().size() == 2);
    REQUIRE(restored.loopCubes.size() == 1);
    REQUIRE(restored.sustainCubes.size() == 1);
    REQUIRE(restored.loopCubes.count(restored.getCubes()[0]) == 1);
    REQUIRE(restored.sustainCubes.count(restored.getCubes()[1]) == 1);
    REQUIRE(restored.getCubes()[0]->getVertex(0)->values[Vertex::Amp] == Catch::Approx(0.1f));
    REQUIRE(restored.getCubes()[0]->getVertex(2)->values[Vertex::Amp] == Catch::Approx(0.8f));
    REQUIRE(restored.getCubes()[1]->getVertex(0)->values[Vertex::Phase] == Catch::Approx(0.75f));

    source.destroy();
    restored.destroy();
}

TEST_CASE("Envelope mesh snapshots reject absent and malformed state", "[cycle-v2][envelope]") {
    EnvelopeMesh mesh("Envelope");

    REQUIRE_FALSE(EnvelopeMeshState::apply({}, mesh));
    REQUIRE_FALSE(EnvelopeMeshState::apply("not-json", mesh));
}
