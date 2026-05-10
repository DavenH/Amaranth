#pragma once

#include <algorithm>
#include <vector>

#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Core/PaddingPolicy.h"
#include "../Policies/Curves/CurvePolicies.h"
#include "../Policies/Curves/WaveformBakePolicy.h"
#include "../RasterizationRequest.h"
#include "../RenderResult.h"
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

        const RenderResult& renderIntercepts(
                std::vector<Intercept>& intercepts,
                RenderResult& output,
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider = nullptr,
                GuideCurveOffsetSeeds* offsetSeeds = nullptr) {
            output.clear();
            output.paddingSize = 2;

            if (intercepts.empty()) {
                return output;
            }

            std::sort(intercepts.begin(), intercepts.end());
            buildCurves(intercepts, output, request);

            if (output.curves.size() < 2) {
                return output;
            }

            Context context;
            context.request = &request;
            context.offsetSeeds = offsetSeeds != nullptr ? offsetSeeds : &fallbackOffsetSeeds;
            context.guideCurveProvider = guideCurveProvider;
            context.guideCurveRegions = &output.guideCurveRegions;
            context.paddingSize = output.paddingSize;

            output.sampleable = render(
                    output.curves,
                    context,
                    [&output](int totalRes) {
                        output.waveform.place(output.waveformMemory, totalRes);
                        return WaveformBufferRefs(output.waveform);
                    });

            return output;
        }

    private:
        static void buildCurves(
                std::vector<Intercept>& intercepts,
                RenderResult& output,
                const RasterizationRequest& request) {
            if (request.cyclic) {
                PaddingPolicyContext context;
                context.interceptPadding = request.interceptPadding;
                output.paddingSize = CyclicPaddingPolicy(context).build(
                        intercepts,
                        output.curves,
                        output.frontPadding,
                        output.backPadding);
            } else {
                PaddingPolicyContext context;
                context.minimumX = request.xMinimum;
                context.maximumX = request.xMaximum;
                output.paddingSize = NonCyclicPaddingPolicy(context).build(intercepts, output.curves);
            }
        }

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
        GuideCurveOffsetSeeds fallbackOffsetSeeds;
    };
}
