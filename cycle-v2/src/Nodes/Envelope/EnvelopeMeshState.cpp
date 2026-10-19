#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Obj/MorphPosition.h>

#include <algorithm>

#include "Nodes/Envelope/EnvelopeMeshState.h"
#include "Nodes/Envelope/EnvelopePurpose.h"

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

void EnvelopeMeshState::initialiseDefault(
        EnvelopeMesh& mesh,
        EnvelopePurpose purpose) {
    if (purpose != EnvelopePurpose::Pitch && purpose != EnvelopePurpose::Scratch) {
        initialiseDefault(mesh);
        return;
    }

    mesh.destroy();
    const float startAmplitude = purpose == EnvelopePurpose::Pitch ? 0.5f : 0.f;
    const float endAmplitude = purpose == EnvelopePurpose::Pitch ? 0.5f : 1.f;
    const float curve = purpose == EnvelopePurpose::Pitch ? -1.f : 1.f;
    addCube(mesh, 0.f, startAmplitude, curve);
    addCube(mesh, 0.999f, endAmplitude, curve);
    mesh.setSustainToLast();
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

bool EnvelopeMeshState::replacePointPath(
        EnvelopeMesh& mesh,
        const std::vector<Vertex2>& points) {
    if (points.size() < 2) {
        return false;
    }
    mesh.destroy();
    for (const auto& point : points) {
        addCube(mesh, point.x, point.y, 0.5f);
    }
    return true;
}

std::vector<Vertex2> EnvelopeMeshState::pointPath(const EnvelopeMesh& mesh) {
    std::vector<Vertex2> points;
    points.reserve(mesh.getCubes().size());
    for (const VertCube* cube : mesh.getCubes()) {
        if (cube != nullptr) {
            const Vertex* vertex = cube->getVertex(0);
            points.emplace_back(vertex->values[Vertex::Phase],
                    vertex->values[Vertex::Amp]);
        }
    }
    std::stable_sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
        return left.x < right.x;
    });
    return points;
}

}
