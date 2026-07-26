#include "PreparedTrimeshTopology.h"

#include "TrimeshMeshFactory.h"

namespace CycleV2 {

PreparedTrimeshTopology::PreparedTrimeshTopology(const String& meshName) : name(meshName) {}

PreparedTrimeshTopology::~PreparedTrimeshTopology() {
    if (preparedMesh != nullptr) {
        preparedMesh->destroy();
    }
}

Mesh& PreparedTrimeshTopology::mesh() {
    if (preparedMesh == nullptr) {
        preparedMesh = TrimeshMeshFactory::createDefaultMesh(name);
    }

    return *preparedMesh;
}

}
