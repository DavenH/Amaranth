#pragma once

#include <algorithm>
#include <vector>

#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Core/InterceptRestrictionPolicy.h"
#include "../Policies/Core/PaddingPolicy.h"
#include "../Policies/Core/PointScalingPolicy.h"
#include "CurveWaveformPipeline.h"
#include "../RasterizationRequest.h"
#include "../WaveformBuffers.h"
#include "../Sources/VertexListSource.h"
#include "../../Curve.h"
#include "../../../Array/ScopedAlloc.h"

namespace Rasterization {
    class FxRasterizationPipeline {
    public:
        struct Output {
            std::vector<Intercept> intercepts;
            std::vector<Curve> curves;

            ScopedAlloc<float> memory;
            WaveformBuffers waveform;

            int paddingSize { 1 };
            bool sampleable {};
        };

        const Output& render(const VertexListSource& source, const RasterizationRequest& request) {
            output = Output();

            if (source.empty()) {
                return output;
            }

            source.copyInterceptsTo(output.intercepts);
            scale(output.intercepts, request.scalingMode);

            if (output.intercepts.empty()) {
                return output;
            }

            std::sort(output.intercepts.begin(), output.intercepts.end());
            restrict(output.intercepts, request);

            if (output.intercepts.size() < 2) {
                return output;
            }

            output.paddingSize = FxPaddingPolicy().build(output.intercepts, output.curves);
            bakeWaveform(request);

            return output;
        }

    private:
        static void scale(std::vector<Intercept>& intercepts, PointScalingMode scalingMode) {
            PointScalingPolicy pointScaling(scalingMode);

            for (auto& intercept : intercepts) {
                intercept.y = pointScaling.scale(intercept.y);
            }
        }

        static void restrict(std::vector<Intercept>& intercepts, const RasterizationRequest& request) {
            InterceptRestrictionPolicy::Context context;
            context.cyclic = false;
            context.minimumX = request.xMinimum;
            context.maximumX = request.xMaximum;

            InterceptRestrictionPolicy(context).restrict(intercepts);
        }

        void bakeWaveform(const RasterizationRequest& request) {
            CurveWaveformPipeline::Context context;
            context.request = &request;
            context.offsetSeeds = &guideCurveOffsetSeeds;
            context.paddingSize = output.paddingSize;

            output.sampleable = curveWaveformPipeline.render(
                    output.curves,
                    context,
                    [this](int totalRes) {
                        output.waveform.place(output.memory, totalRes);
                        return WaveformBufferRefs(output.waveform);
                    });
        }

        Output output;
        CurveWaveformPipeline curveWaveformPipeline;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    };
}
