#pragma once

#include "../../Graph/NodeDefinition.h"

#include <Curve/Mesh/Mesh.h>

#include <memory>
#include <vector>

namespace CycleV2 {

class PreparedTrimeshTopology {
public:
    explicit PreparedTrimeshTopology(const String& meshName);
    ~PreparedTrimeshTopology();

    Mesh& mesh(const std::vector<NodeParameter>& parameters);

private:
    void rebuild(const String& serializedState);

    String name;
    String appliedState;
    std::unique_ptr<Mesh> preparedMesh;
};

}
