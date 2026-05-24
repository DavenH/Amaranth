#include "TrimeshMeshFactory.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>

namespace CycleV2 {

std::unique_ptr<Mesh> TrimeshMeshFactory::createDefaultMesh(const juce::String& name) {
    auto mesh = std::make_unique<Mesh>(name);

    addVoiceCube(*mesh, 0.00f, 0.10f, 0.50f, 0.82f, 0.24f);
    addVoiceCube(*mesh, 0.10f, 0.18f, 0.82f, 0.24f, 0.62f);
    addVoiceCube(*mesh, 0.18f, 0.31f, 0.24f, 0.76f, 0.36f);
    addVoiceCube(*mesh, 0.31f, 0.43f, 0.76f, 0.18f, 0.70f);
    addVoiceCube(*mesh, 0.43f, 0.57f, 0.18f, 0.66f, 0.42f);
    addVoiceCube(*mesh, 0.57f, 0.70f, 0.66f, 0.40f, 0.58f);
    addVoiceCube(*mesh, 0.70f, 0.84f, 0.40f, 0.92f, 0.30f);
    addVoiceCube(*mesh, 0.84f, 1.00f, 0.92f, 0.36f, 0.66f);

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
        const float redOffset = red ? 0.08f : -0.05f;
        const float blueOffset = blue ? -0.06f : 0.04f;
        const float timeOffset = time ? 0.03f : -0.02f;

        vertex->values[Vertex::Time] = time ? 1.f : 0.f;
        vertex->values[Vertex::Red] = red ? 1.f : 0.f;
        vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
        vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
        vertex->values[Vertex::Amp] = juce::jlimit(
                0.f,
                1.f,
                (time ? highAmp : lowAmp) + redOffset + blueOffset + timeOffset);
        vertex->values[Vertex::Curve] = sharpness;
    }
}

}
