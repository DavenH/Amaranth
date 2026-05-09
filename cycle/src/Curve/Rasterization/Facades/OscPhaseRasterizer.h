#pragma once

#include <Curve/MeshRasterizer.h>

class Mesh;

namespace Cycle::Rasterization {
    class OscPhaseRasterizer {
    public:
        explicit OscPhaseRasterizer(const String& name = String()) :
                rasterizer(name) {
        }

        void rasterize(Mesh* mesh) {
            rasterizer.setMesh(mesh);
            rasterizer.calcCrossPoints();
        }

    private:
        MeshRasterizer rasterizer;
    };
}
