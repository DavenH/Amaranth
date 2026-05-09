#pragma once

#include <vector>

#include "../Builders/TransferTable.h"
#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Curves/CurveResolutionPolicy.h"
#include "../Policies/Curves/WaveformBuildPolicy.h"
#include "../RasterizationRequest.h"
#include "../../Curve.h"

namespace Rasterization {
    class CurveWaveformPipeline {
    public:
        struct Context {
            const RasterizationRequest* request {};
            GuideCurveOffsetSeeds* offsetSeeds {};
            int paddingSize { 2 };
        };

        template<typename AllocateTarget>
        bool render(std::vector<Curve>& curves, Context context, AllocateTarget allocateTarget) const {
            jassert(context.request != nullptr);
            jassert(context.offsetSeeds != nullptr);

            applyResolution(curves, *context.request, context.paddingSize);
            return bakeWaveform(curves, *context.request, *context.offsetSeeds, allocateTarget);
        }

    private:
        static void applyResolution(
                std::vector<Curve>& curves,
                const RasterizationRequest& request,
                int paddingSize) {
            CurveResolutionPolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.integralSampling = request.integralSampling;
            context.interpolateCurves = request.interpolateCurves;
            context.paddingSize = paddingSize;
            CurveResolutionPolicy().apply(curves, context);

            for (auto& curve : curves) {
                curve.recalculateCurve();
            }
        }

        template<typename AllocateTarget>
        bool bakeWaveform(
                std::vector<Curve>& curves,
                const RasterizationRequest& request,
                GuideCurveOffsetSeeds& offsetSeeds,
                AllocateTarget allocateTarget) const {
            WaveformBakePolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.morph = request.morph;
            context.offsetSeeds = &offsetSeeds;
            context.transferTable = TransferTable::values();

            return waveformBuilder.build(curves, context, allocateTarget);
        }

        WaveformBuildPolicy waveformBuilder;
    };
}
