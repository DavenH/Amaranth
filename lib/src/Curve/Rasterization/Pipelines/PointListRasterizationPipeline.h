#pragma once

#include <vector>

#include "../GuideCurveOffsetSeeds.h"
#include "../Policies/Core/PaddingPolicy.h"
#include "CurveWaveformPipeline.h"
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

            bakeWaveform(request);

            return output;
        }

    private:
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
