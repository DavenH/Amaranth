#pragma once

#include <vector>

#include "../Builders/RasterizerSnapshotBuilder.h"
#include "../Builders/WaveformBuilder.h"
#include "../Policies/CurveResolutionPolicy.h"

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

        int prepareWaveform(std::vector<Curve>& curves, const WaveformBakePolicy::Context& context) const {
            return waveformBuilder.prepare(curves, context);
        }

        void bakeWaveform(std::vector<Curve>& curves, WaveformBakePolicy::Context& context) const {
            waveformBuilder.bake(curves, context);
        }

        void publishSnapshot(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            RasterizerSnapshotBuilder().publish(target, source);
        }

        void skipSnapshot(RasterizerData& target, const RasterizerSnapshotSource& source) const {
            RasterizerSnapshotBuilder<NoSnapshot>().publish(target, source);
        }

    private:
        CurveResolutionPolicy curveResolutionPolicy;
        WaveformBuilder waveformBuilder;
    };
}
