#include "TrimeshMeshState.h"

#include <Curve/Mesh/Mesh.h>

#include "TrimeshMeshFactory.h"

namespace CycleV2 {

TrimeshNodeModelState::TrimeshNodeModelState(var meshStateToUse, uint64_t revisionToUse) :
        meshState(std::move(meshStateToUse))
    ,   modelRevision(revisionToUse) {}

String TrimeshNodeModelState::schemaId() const {
    return "trimesh";
}

int TrimeshNodeModelState::schemaVersion() const {
    return 2;
}

uint64_t TrimeshNodeModelState::revision() const {
    return modelRevision;
}

var TrimeshNodeModelState::writeJSON() const {
    auto result = std::make_unique<DynamicObject>();
    result->setProperty("schema", schemaId());
    result->setProperty("version", schemaVersion());
    result->setProperty("revision", (int64) modelRevision);
    result->setProperty("mesh", meshState);
    return var(result.release());
}

bool TrimeshNodeModelState::equals(const NodeModelState& other) const {
    const auto* typed = dynamic_cast<const TrimeshNodeModelState*>(&other);
    return typed != nullptr
            && modelRevision == typed->modelRevision
            && JSON::toString(meshState, false) == JSON::toString(typed->meshState, false);
}

String TrimeshNodeModelCodec::schemaId() const {
    return "trimesh";
}

int TrimeshNodeModelCodec::currentVersion() const {
    return 2;
}

NodeModelStatePtr TrimeshNodeModelCodec::createDefault() const {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
    var state = mesh->writeJSON();
    mesh->destroy();
    return std::make_shared<const TrimeshNodeModelState>(std::move(state), 1);
}

NodeModelStatePtr TrimeshNodeModelCodec::readJSON(const var& value, String& error) const {
    const auto* object = value.getDynamicObject();
    if (object == nullptr || object->getProperty("schema").toString() != schemaId()) {
        error = "Expected Trimesh model schema 'trimesh'";
        return nullptr;
    }
    if ((int) object->getProperty("version") != currentVersion()) {
        error = "Unsupported Trimesh model schema version";
        return nullptr;
    }
    const int64 revision = object->getProperty("revision");
    if (revision < 1) {
        error = "Trimesh model revision must be positive";
        return nullptr;
    }

    const var meshState = object->getProperty("mesh");
    const auto* meshObject = meshState.getDynamicObject();
    const auto* vertices = meshObject != nullptr
            ? meshObject->getProperty("vertices").getArray()
            : nullptr;
    const auto* cubes = meshObject != nullptr
            ? meshObject->getProperty("cubes").getArray()
            : nullptr;
    if (vertices == nullptr || cubes == nullptr) {
        error = "Trimesh mesh state is incomplete";
        return nullptr;
    }
    Mesh validated;
    if (!validated.readJSON(meshState)
            || validated.getNumVerts() != vertices->size()
            || validated.getNumCubes() != cubes->size()) {
        validated.destroy();
        error = "Invalid Trimesh mesh state";
        return nullptr;
    }
    var canonicalState = validated.writeJSON();
    validated.destroy();
    return std::make_shared<const TrimeshNodeModelState>(
            std::move(canonicalState), (uint64_t) revision);
}

}
