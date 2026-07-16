#pragma once

#include <climits>
#include <vector>

#include "Sampling/GuideCurveSampler.h"
#include "WaveformBuffers.h"
#include "../Curve.h"
#include <Curve/Mesh/Intercept.h>
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
        bool fixedWaveformCapacity {};

        bool placeWaveform(int size) {
            if (fixedWaveformCapacity) {
                return waveform.placeInPreparedMemory(waveformMemory, size);
            }

            waveform.place(waveformMemory, size);
            return true;
        }

        void clearDerivedOutput() {
            frontPadding.clear();
            backPadding.clear();
            curves.clear();
            guideCurveRegions.clear();
            waveform.nullify();
            paddingSize = 1;
            sampleable = false;
            needsResorting = false;
        }

        void clear() {
            intercepts.clear();
            colorPoints.clear();
            clearDerivedOutput();
        }
    };
}
