#include "PreparedTrimeshTopology.h"

#include "TrimeshMeshFactory.h"
#include "TrimeshMeshState.h"

namespace CycleV2 {

PreparedTrimeshTopology::PreparedTrimeshTopology(const String& meshName) : name(meshName) {}

PreparedTrimeshTopology::~PreparedTrimeshTopology() {
    if (preparedMesh != nullptr) {
        preparedMesh->destroy();
    }
}

Mesh& PreparedTrimeshTopology::mesh(const std::vector<NodeParameter>& parameters) {
    const String serializedState = typedParameterString(
            parameters,
            TrimeshMeshState::parameterId(),
            {});
    if (preparedMesh == nullptr || serializedState != appliedState) {
        rebuild(serializedState);
    }

    return *preparedMesh;
}

void PreparedTrimeshTopology::rebuild(const String& serializedState) {
    if (preparedMesh != nullptr) {
        preparedMesh->destroy();
    }

    preparedMesh = TrimeshMeshFactory::createDefaultMesh(name);
    if (serializedState.isNotEmpty()) {
        TrimeshMeshState::apply(serializedState, *preparedMesh);
    }
    appliedState = serializedState;
}

}
