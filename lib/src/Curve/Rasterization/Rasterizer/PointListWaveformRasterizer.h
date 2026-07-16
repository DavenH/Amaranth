#pragma once

#include <algorithm>
#include <vector>

#include <Curve/Rasterization/Builders/CurveWaveformBuilder.h>
#include <Curve/Rasterization/GuideCurveOffsetSeeds.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Core/PaddingPolicy.h>
#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/RasterizationRequest.h>
#include <Curve/Rasterization/RenderResult.h>

#include "RasterizerViews.h"

namespace Rasterization {
    enum class PointListPaddingMode {
        Request,
        Fx
    };

    class PointListWaveformRasterizer {
    public:
        struct Diagnostics {
            int fullBakeCount {};
            int affectedRangeBakeCount {};
        };

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
            ++diagnostics.fullBakeCount;
            builder.renderIntercepts(
                    intercepts,
                    output,
                    request,
                    guideCurveProvider,
                    &guideCurveOffsetSeeds);

            return output;
        }

        bool rebakeAffectedRange(
                int firstCurve,
                int endCurve,
                const RasterizationRequest& request) {
            const int baseResolution = Curve::resolution / 2;
            const float resolutionThreshold = 0.1f / float(Curve::resolution);
            const int firstResolutionCurve = jmax(1, firstCurve - 1);
            const int endResolutionCurve = jmin(
                    (int) output.curves.size() - 1,
                    endCurve + 1);

            for (int curveIndex = firstResolutionCurve;
                    curveIndex < endResolutionCurve;
                    ++curveIndex) {
                const float width = output.curves[curveIndex + 1].c.x
                                  - output.curves[curveIndex - 1].a.x;
                int expectedIndex = 0;
                for (int resolutionIndex = 0;
                        resolutionIndex < Curve::resolutions;
                        ++resolutionIndex) {
                    const int resolution = Curve::resolution >> resolutionIndex;
                    if (width < resolutionThreshold * float(resolution)) {
                        expectedIndex = resolutionIndex;
                    }
                }

                if (output.curves[curveIndex].resIndex != expectedIndex) {
                    return false;
                }
            }

            for (int curveIndex = firstCurve; curveIndex < endCurve; ++curveIndex) {
                const Curve& curve = output.curves[curveIndex];
                const Curve& next = output.curves[curveIndex + 1];
                const int expectedResolution = jmin(
                        baseResolution >> curve.resIndex,
                        baseResolution >> next.resIndex);
                if (curve.curveRes != expectedResolution) {
                    return false;
                }
            }

            builder.rebakeAffectedRange(output, request, firstCurve, endCurve);
            ++diagnostics.affectedRangeBakeCount;
            return true;
        }

        const Diagnostics& getDiagnostics() const {
            return diagnostics;
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
        Diagnostics diagnostics;
    };
}
