#pragma once

#include <vector>

#include "../Builders/RasterizerSnapshotBuilder.h"
#include "../Policies/Curves/CurveResolutionPolicy.h"

namespace Rasterization {
    class MeshRasterizerFacade {
    public:
        void applyCurveResolution(
                std::vector<Curve>& curves,
                const CurveResolutionPolicy::Context& context) const {
            curveResolutionPolicy.apply(curves, context);
        }

        void applyResolutionIndices(std::vector<Curve>& curves, float base, int paddingSize) const {
            CurveResolutionPolicy::applyResolutionIndices(curves, base, paddingSize);
        }

        void publishSnapshot(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            RasterizerSnapshotBuilder().publish(target, source);
        }

        void skipSnapshot(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            RasterizerSnapshotBuilder<NoSnapshot>().publish(target, source);
        }

    private:
        CurveResolutionPolicy curveResolutionPolicy;
    };
}
