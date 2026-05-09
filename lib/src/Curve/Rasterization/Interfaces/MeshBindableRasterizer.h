#pragma once

class Mesh;

namespace Rasterization {
    class MeshBindableRasterizer {
    public:
        virtual ~MeshBindableRasterizer() = default;

        virtual Mesh* getMesh() = 0;
        virtual void setMesh(Mesh* mesh) = 0;
        virtual bool hasEnoughCubesForCrossSection() = 0;
    };
}
