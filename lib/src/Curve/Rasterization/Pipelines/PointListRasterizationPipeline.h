#pragma once

#include <vector>

#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Core/PaddingPolicy.h"
#include "../RenderResult.h"
#include "CurveWaveformPipeline.h"
#include "../RasterizationRequest.h"
#include "../Sources/PointListSource.h"
#include "../../Curve.h"

namespace Rasterization {
    class PointListRasterizationPipeline {
    public:
        const RenderResult& render(
                std::vector<Intercept>& points,
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider = nullptr,
                GuideCurveOffsetSeeds* offsetSeeds = nullptr) {
            output.clear();
            output.paddingSize = 2;

            PointListSource source(points);
            if (source.empty()) {
                return output;
            }

            source.sortByX();
            std::vector<Intercept>& intercepts = source.intercepts();

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

            if (output.curves.size() < 2) {
                return output;
            }

            bakeWaveform(request, guideCurveProvider, offsetSeeds);

            return output;
        }

    private:
        void bakeWaveform(
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider,
                GuideCurveOffsetSeeds* offsetSeeds) {
            CurveWaveformPipeline::Context context;
            context.request = &request;
            context.offsetSeeds = offsetSeeds != nullptr ? offsetSeeds : &guideCurveOffsetSeeds;
            context.guideCurveProvider = guideCurveProvider;
            context.guideCurveRegions = &output.guideCurveRegions;
            context.paddingSize = output.paddingSize;

            output.sampleable = curveWaveformPipeline.render(
                    output.curves,
                    context,
                    [this](int totalRes) {
                        output.waveform.place(output.waveformMemory, totalRes);
                        return WaveformBufferRefs(output.waveform);
                    });
        }

        RenderResult output;
        CurveWaveformPipeline curveWaveformPipeline;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    };
}
