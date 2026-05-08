#pragma once

#include "RasterizerSnapshotProvider.h"
#include "../../MeshRasterizer.h"

namespace Rasterization {
    class MeshRasterizerSnapshotAdapter :
            public RasterizerSnapshotProvider {
    public:
        MeshRasterizerSnapshotAdapter() = default;

        explicit MeshRasterizerSnapshotAdapter(MeshRasterizer* rasterizer) :
                rasterizer(rasterizer) {
        }

        void setRasterizer(MeshRasterizer* newRasterizer) {
            rasterizer = newRasterizer;
        }

        MeshRasterizer* getRasterizer() const { return rasterizer; }

        RasterizerData& getRasterizerData() override {
            jassert(rasterizer != nullptr);
            return rasterizer->getRastData();
        }

        const RasterizerData& getRasterizerData() const override {
            jassert(rasterizer != nullptr);
            return rasterizer->getRastData();
        }

    private:
        MeshRasterizer* rasterizer {};
    };
}
