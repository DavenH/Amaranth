#include "EnvelopeMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Obj/MorphPosition.h>

namespace CycleV2 {

namespace {

VertCube* addCube(EnvelopeMesh& mesh, float x, float amp, float curve) {
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
        vertex->values[Vertex::Amp] = amp;

        if (curve >= 0.f) {
            vertex->values[Vertex::Curve] = curve;
        }
    }

    mesh.addCube(cube);
    return cube;
}

}

void EnvelopeMeshState::initialiseDefault(EnvelopeMesh& mesh) {
    mesh.destroy();
    addCube(mesh, 0.f, 0.f, 1.f);
    addCube(mesh, 0.05f, 1.f, 0.5f);
    VertCube* morphCube = addCube(mesh, 0.7f, 0.8f, 0.3f);

    for (int i = 0; i < (int) VertCube::numVerts; ++i) {
        Vertex* vertex = morphCube->getVertex(i);

        if (vertex->values[Vertex::Red] > 0.5f) {
            vertex->values[Vertex::Amp] = 0.42f;
        }

        if (vertex->values[Vertex::Blue] > 0.5f) {
            vertex->values[Vertex::Amp] = 0.94f;
        }
    }

    addCube(mesh, 0.999f, 0.8f, 1.f);
    mesh.setSustainToLast();
    addCube(mesh, 1.075f, 0.6f, -1.f);
    addCube(mesh, 1.15f, 0.f, -1.f);
    addCube(mesh, 1.25f, 0.f, -1.f);
}

juce::String EnvelopeMeshState::defaultSnapshot() {
    EnvelopeMesh mesh("CycleV2Envelope");
    initialiseDefault(mesh);
    const juce::String result = serialize(mesh);
    mesh.destroy();
    return result;
}

juce::String EnvelopeMeshState::serialize(const EnvelopeMesh& mesh) {
    return juce::JSON::toString(mesh.writeJSON(), false);
}

bool EnvelopeMeshState::apply(const juce::String& serialized, EnvelopeMesh& mesh) {
    if (serialized.isEmpty()) {
        return false;
    }

    const juce::var snapshot = juce::JSON::parse(serialized);
    if (snapshot.isVoid() || snapshot.isUndefined()) {
        return false;
    }

    return mesh.readJSON(snapshot);
}

}
