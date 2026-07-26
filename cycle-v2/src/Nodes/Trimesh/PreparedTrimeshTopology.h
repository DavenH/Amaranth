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

    Mesh& mesh();

private:
    String name;
    std::unique_ptr<Mesh> preparedMesh;
};

}
