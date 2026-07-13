#include "EnvelopeMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>

namespace CycleV2 {

juce::String EnvelopeMeshState::parameterId() {
    return "envelope.snapshot";
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
