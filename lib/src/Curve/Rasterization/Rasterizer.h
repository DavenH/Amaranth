#pragma once

#include "../../Design/Updating/UpdateType.h"
#include "RasterizerViews.h"

class Mesh;
struct RasterizerData;

namespace Rasterization {
    class Rasterizer {
    public:
        virtual ~Rasterizer() = default;

        virtual void setMesh(Mesh* mesh) = 0;

        virtual SamplerView samplerView() const = 0;
        virtual SnapshotView snapshotView() = 0;

        virtual void performUpdate(UpdateType updateType) = 0;

    };
}
