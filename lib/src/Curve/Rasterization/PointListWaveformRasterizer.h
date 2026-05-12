#pragma once

#include <algorithm>
#include <vector>

#include "Builders/CurveWaveformBuilder.h"
#include "GuideCurveOffsetSeeds.h"
#include "Policies/Core/InterceptPolicies.h"
#include "Policies/Core/PaddingPolicy.h"
#include "Policies/Core/PointScalingPolicy.h"
#include "RasterizationRequest.h"
#include "RasterizerViews.h"
#include "RenderResult.h"

namespace Rasterization {
    enum class PointListPaddingMode {
        Request,
        Fx
    };

    class PointListWaveformRasterizer {
    public:
        RenderResult& result() { return output; }
        const RenderResult& result() const { return output; }

        SamplerView sampler() const {
            return SamplerView(output.waveform, output.sampleable);
        }

        void cleanUp() {
            output.clear();
        }

        const RenderResult& renderIntercepts(
                std::vector<Intercept>& intercepts,
                const RasterizationRequest& request,
                GuideCurveProvider* guideCurveProvider = nullptr) {
            builder.renderIntercepts(
                    intercepts,
                    output,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);

            return output;
        }

        const RenderResult& renderGeometry(
                std::vector<Intercept>& intercepts,
                const RasterizationRequest& request,
                PointListPaddingMode paddingMode = PointListPaddingMode::Request) {
            output.clear();

            if (intercepts.empty()) {
                return output;
            }

            output.intercepts = intercepts;

            PointScalingPolicy pointScaling(request.scalingMode);
            for (auto& intercept : output.intercepts) {
                intercept.y = pointScaling.scale(intercept.y);
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());

            InterceptRestrictionPolicy::Context context;
            context.cyclic = false;
            context.minimumX = request.xMinimum;
            context.maximumX = request.xMaximum;
            InterceptRestrictionPolicy(context).restrict(output.intercepts);

            if (output.intercepts.size() < 2) {
                return output;
            }

            buildPadding(request, paddingMode);

            return output;
        }

        const RenderResult& renderWaveform(
                std::vector<Intercept>& intercepts,
                const RasterizationRequest& request,
                PointListPaddingMode paddingMode = PointListPaddingMode::Request) {
            renderGeometry(intercepts, request, paddingMode);

            if (output.curves.size() < 2) {
                return output;
            }

            CurveWaveformBuilder::Context context;
            context.request = &request;
            context.offsetSeeds = &guideCurveOffsetSeeds;
            context.paddingSize = output.paddingSize;

            output.sampleable = builder.render(
                    output.curves,
                    context,
                    [this](int totalRes) {
                        output.waveform.place(output.waveformMemory, totalRes);
                        return WaveformBufferRefs(output.waveform);
                    });

            return output;
        }

    private:
        void buildPadding(
                const RasterizationRequest& request,
                PointListPaddingMode paddingMode) {
            switch (paddingMode) {
                case PointListPaddingMode::Fx:
                    output.paddingSize = FxPaddingPolicy().build(output.intercepts, output.curves);
                    return;
                case PointListPaddingMode::Request:
                    if (request.cyclic) {
                        PaddingPolicyContext context;
                        context.interceptPadding = request.interceptPadding;
                        output.paddingSize = CyclicPaddingPolicy(context).build(
                                output.intercepts,
                                output.curves,
                                output.frontPadding,
                                output.backPadding);
                    } else {
                        PaddingPolicyContext context;
                        context.minimumX = request.xMinimum;
                        context.maximumX = request.xMaximum;
                        output.paddingSize = NonCyclicPaddingPolicy(context).build(
                                output.intercepts,
                                output.curves);
                    }
                    return;
            }
        }

        RenderResult output;
        CurveWaveformBuilder builder;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    };
}
