#pragma once

#include "../../Design/Updating/UpdateType.h"
#include "RasterizerViews.h"

class Dimensions;
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

        virtual bool wrapsVertices() const = 0;
        virtual int getPaddingSize() const = 0;
        virtual void setDims(const Dimensions& dims) = 0;
    };
}
