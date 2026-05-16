#pragma once

#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>

#include "Rasterizer.h"
#include "RasterizerData.h"

namespace Rasterization {
    class BaseRasterizer : public Rasterizer {
    public:
        SnapshotView snapshotView() override {
            return SnapshotView(rasterizerData);
        }

    protected:
        void publishSnapshot(const RasterizerSnapshotSource& source) {
            RasterizerSnapshotBuilder().publish(rasterizerData, source);
        }

        RasterizerData rasterizerData;
    };
}
