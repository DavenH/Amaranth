#pragma once

#include "../../Design/Updating/UpdateType.h"
#include "RasterizerViews.h"

namespace Rasterization {
    class Rasterizer {
    public:
        virtual ~Rasterizer() = default;

        virtual SamplerView samplerView() const = 0;
        virtual SnapshotView snapshotView() = 0;

        virtual void performUpdate(UpdateType updateType) = 0;
    };
}
