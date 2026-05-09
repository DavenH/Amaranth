#pragma once

#include "../../../Design/Updating/UpdateType.h"

namespace Rasterization {
    class RasterizerUpdateTarget {
    public:
        virtual ~RasterizerUpdateTarget() = default;

        virtual void cleanUp() = 0;
        virtual void performUpdate(UpdateType updateType) = 0;
        virtual void reset() = 0;
        virtual void updateRasterizer(UpdateType updateType) = 0;
    };
}
