#include "TrimeshMeshFactory.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>

namespace CycleV2 {

std::unique_ptr<Mesh> TrimeshMeshFactory::createDefaultMesh(const juce::String& name) {
    auto mesh = std::make_unique<Mesh>(name);

    addVoiceCube(*mesh, 0.00f, 0.50f, 0.50f, 0.85f, 0.35f);
    addVoiceCube(*mesh, 0.50f, 1.00f, 0.85f, 0.15f, 0.45f);

    return mesh;
}

VertCube* TrimeshMeshFactory::addVoiceCube(
        Mesh& mesh,
        float lowPhase,
        float highPhase,
        float lowAmp,
        float highAmp,
        float sharpness) {
    auto* cube = new VertCube(&mesh);
    configureVoiceCube(*cube, lowPhase, highPhase, lowAmp, highAmp, sharpness);
    mesh.addCube(cube);

    return cube;
}

void TrimeshMeshFactory::configureVoiceCube(
        VertCube& cube,
        float lowPhase,
        float highPhase,
        float lowAmp,
        float highAmp,
        float sharpness) {
    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        bool time {};
        bool red {};
        bool blue {};
        VertCube::getPoles(i, time, red, blue);

        Vertex* vertex = cube.getVertex(i);
        vertex->values[Vertex::Time] = time ? 1.f : 0.f;
        vertex->values[Vertex::Red] = red ? 1.f : 0.f;
        vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
        vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
        vertex->values[Vertex::Amp] = time ? highAmp : lowAmp;
        vertex->values[Vertex::Curve] = sharpness;
    }
}

}
