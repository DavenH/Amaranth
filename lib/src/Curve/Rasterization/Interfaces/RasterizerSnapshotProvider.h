#pragma once

struct RasterizerData;

namespace Rasterization {
    class RasterizerSnapshotProvider {
    public:
        virtual ~RasterizerSnapshotProvider() = default;

        virtual RasterizerData& getRasterizerData() = 0;
        virtual const RasterizerData& getRasterizerData() const = 0;
    };
}
