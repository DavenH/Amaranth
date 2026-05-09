#pragma once

#include <vector>

#include "../Builders/TransferTable.h"
#include "../Builders/WaveformBuilder.h"
#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Curves/CurveResolutionPolicy.h"
#include "../Policies/Core/PaddingPolicy.h"
#include "../Policies/Curves/WaveformBuildPolicy.h"
#include "../RasterizationRequest.h"
#include "../WaveformBuffers.h"
#include "../Sources/PointListSource.h"
#include "../../Curve.h"
#include "../../../Array/ScopedAlloc.h"

namespace Rasterization {
    class PointListRasterizationPipeline {
    public:
        struct Output {
            std::vector<Intercept> frontPadding;
            std::vector<Intercept> backPadding;
            std::vector<Curve> curves;

            ScopedAlloc<float> memory;
            WaveformBuffers waveform;

            int paddingSize { 2 };
            bool sampleable {};
        };

        const Output& render(std::vector<Intercept>& points, const RasterizationRequest& request) {
            output = Output();

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

            CurveResolutionPolicy::Context resolutionContext;
            resolutionContext.lowResCurves = request.lowResCurves;
            resolutionContext.integralSampling = request.integralSampling;
            resolutionContext.interpolateCurves = request.interpolateCurves;
            resolutionContext.paddingSize = output.paddingSize;
            CurveResolutionPolicy().apply(output.curves, resolutionContext);

            for (auto& curve : output.curves) {
                curve.recalculateCurve();
            }

            bakeWaveform(request);

            return output;
        }

    private:
        void bakeWaveform(const RasterizationRequest& request) {
            WaveformBakePolicy::Context context;
            context.lowResCurves = request.lowResCurves;
            context.morph = request.morph;
            context.offsetSeeds = &guideCurveOffsetSeeds;
            context.transferTable = TransferTable::values();

            output.sampleable = waveformBuilder.build(
                    output.curves,
                    context,
                    [this](int totalRes) {
                        output.waveform.place(output.memory, totalRes);
                        return WaveformBufferRefs(output.waveform);
                    });
        }

        Output output;
        WaveformBuildPolicy waveformBuilder;
        GuideCurveOffsetSeeds guideCurveOffsetSeeds;
    };
}
