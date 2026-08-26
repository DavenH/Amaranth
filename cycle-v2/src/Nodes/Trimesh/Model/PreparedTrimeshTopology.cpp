#include "Nodes/Trimesh/Model/PreparedTrimeshTopology.h"

#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"

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
