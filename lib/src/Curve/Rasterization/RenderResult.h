#pragma once

#include <climits>
#include <vector>

#include "Sampling/GuideCurveSampler.h"
#include "WaveformBuffers.h"
#include "../Curve.h"
#include "../Intercept.h"
#include "../../Array/ScopedAlloc.h"
#include "../../Obj/ColorPoint.h"

namespace Rasterization {
    struct RenderResult {
        std::vector<Intercept> intercepts;
        std::vector<Intercept> frontPadding;
        std::vector<Intercept> backPadding;
        std::vector<Curve> curves;
        std::vector<GuideCurveRegion> guideCurveRegions;
        std::vector<ColorPoint> colorPoints;

        ScopedAlloc<float> waveformMemory;
        WaveformBuffers waveform { 0, INT_MAX / 2 };

        int paddingSize { 1 };
        bool sampleable {};
        bool needsResorting {};

        void clear() {
            intercepts.clear();
            frontPadding.clear();
            backPadding.clear();
            curves.clear();
            guideCurveRegions.clear();
            colorPoints.clear();
            waveformMemory.clear();
            waveform.nullify();
            paddingSize = 1;
            sampleable = false;
            needsResorting = false;
        }
    };
}
