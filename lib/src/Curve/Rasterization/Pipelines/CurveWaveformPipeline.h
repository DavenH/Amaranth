#pragma once

#include <vector>

#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Curves/CurvePolicies.h"
#include "../Policies/Curves/WaveformBakePolicy.h"
#include "../RasterizationRequest.h"
#include "../../Curve.h"

namespace Rasterization {
    class CurveWaveformPipeline {
    public:
        struct Context {
            const RasterizationRequest* request {};
            GuideCurveOffsetSeeds* offsetSeeds {};
            GuideCurveProvider* guideCurveProvider {};
            std::vector<GuideCurveRegion>* guideCurveRegions {};
            int paddingSize { 2 };
        };

        template<typename AllocateTarget>
        bool render(std::vector<Curve>& curves, Context context, AllocateTarget allocateTarget) const {
            jassert(context.request != nullptr);
            jassert(context.offsetSeeds != nullptr);

            applyResolution(curves, *context.request, context.paddingSize);
            return bakeWaveform(
                    curves,
                    *context.request,
                    *context.offsetSeeds,
                    context.guideCurveProvider,
                    context.guideCurveRegions,
                    allocateTarget);
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

            CurveWaveformPreparationPolicy().apply(curves);
        }

        template<typename AllocateTarget>
        bool bakeWaveform(
                std::vector<Curve>& curves,
                const RasterizationRequest& request,
                GuideCurveOffsetSeeds& offsetSeeds,
                GuideCurveProvider* guideCurveProvider,
                std::vector<GuideCurveRegion>* guideCurveRegions,
                AllocateTarget allocateTarget) const {
            WaveformBakePolicy::Context bakeContext;
            bakeContext.lowResCurves = request.lowResCurves;
            bakeContext.morph = request.morph;
            bakeContext.decoupleComponentDfrms = request.decoupleComponentDeforms;
            bakeContext.noiseSeed = request.noiseSeed;
            bakeContext.guideCurveProvider = guideCurveProvider;
            bakeContext.guideCurveRegions = guideCurveRegions;
            bakeContext.offsetSeeds = &offsetSeeds;

            return waveformBuilder.build(curves, bakeContext, allocateTarget);
        }

        WaveformBakePolicy waveformBuilder;
    };
}
