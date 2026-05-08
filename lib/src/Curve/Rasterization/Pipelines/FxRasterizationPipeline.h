#pragma once

#include <algorithm>
#include <vector>

#include "../Builders/TransferTable.h"
#include "../Builders/WaveformBuilder.h"
#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/CurveResolutionPolicy.h"
#include "../Policies/InterceptRestrictionPolicy.h"
#include "../Policies/PaddingPolicy.h"
#include "../Policies/PointScalingPolicy.h"
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
            applyResolution(request);
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

        void applyResolution(const RasterizationRequest& request) {
            CurveResolutionPolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.integralSampling = request.integralSampling;
            context.interpolateCurves = request.interpolateCurves;
            context.paddingSize = output.paddingSize;
            CurveResolutionPolicy().apply(output.curves, context);

            for (auto& curve : output.curves) {
                curve.recalculateCurve();
            }
        }

        void bakeWaveform(const RasterizationRequest& request) {
            WaveformBakePolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.morph = request.morph;
            context.offsetSeeds = &guideCurveOffsetSeeds;
            context.transferTable = TransferTable::values();

            int totalRes = waveformBuilder.prepare(output.curves, context);
            if (totalRes <= 0) {
                return;
            }

            output.waveform.place(output.memory, totalRes);
            context.waveform = WaveformBufferRefs(output.waveform);

            waveformBuilder.bake(output.curves, context);
            output.sampleable = output.waveform.isSampleable();
        }

        Output output;
        WaveformBuilder waveformBuilder;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    };
}
