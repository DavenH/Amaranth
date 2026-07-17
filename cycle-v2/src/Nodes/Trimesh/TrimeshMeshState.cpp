#include "TrimeshMeshState.h"

#include <App/Doc/PresetJson.h>
#include <Curve/Mesh/Mesh.h>

namespace CycleV2 {

namespace {

bool parseValidated(const juce::String& serialized, Mesh& mesh) {
    if (serialized.isEmpty()) {
        return false;
    }

    const juce::var snapshot = juce::JSON::parse(serialized);
    const auto* vertices = PresetJson::getArray(PresetJson::property(snapshot, "vertices"));
    const auto* cubes = PresetJson::getArray(PresetJson::property(snapshot, "cubes"));
    if (vertices == nullptr || cubes == nullptr) {
        return false;
    }
    if (!mesh.readJSON(snapshot)) {
        return false;
    }
    return mesh.getNumVerts() == vertices->size()
            && mesh.getNumCubes() == cubes->size();
}

}

const juce::String& TrimeshMeshState::parameterId() {
    static const juce::String result { "mesh.topology" };
    return result;
}

juce::String TrimeshMeshState::serialize(const Mesh& mesh) {
    return juce::JSON::toString(mesh.writeJSON(), false);
}

bool TrimeshMeshState::apply(const juce::String& serialized, Mesh& mesh) {
    Mesh validated;
    if (!parseValidated(serialized, validated)) {
        validated.destroy();
        return false;
    }

    mesh.destroy();
    const bool applied = mesh.readJSON(validated.writeJSON());
    validated.destroy();
    return applied;
}

bool TrimeshMeshState::isValid(const juce::String& serialized) {
    Mesh validated;
    const bool result = parseValidated(serialized, validated);
    validated.destroy();
    return result;
}

}
