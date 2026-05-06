#pragma once

#include "../../Mesh.h"

namespace Rasterization {
    class MeshCubeSource {
    public:
        explicit MeshCubeSource(Mesh* mesh = nullptr) :
                mesh(mesh) {
        }

        Mesh* getMesh() const { return mesh; }
        bool empty() const { return mesh == nullptr || mesh->getNumCubes() == 0; }
        int size() const { return mesh == nullptr ? 0 : mesh->getNumCubes(); }
        VertCube* cubeAt(int index) const { return mesh->getCubes()[index]; }

    private:
        Mesh* mesh {};
    };
}
