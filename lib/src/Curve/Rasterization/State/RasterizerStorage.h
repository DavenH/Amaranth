#pragma once

#include <climits>
#include <vector>

#include "../Sampling/GuideCurveSampler.h"
#include "../WaveformBuffers.h"
#include "../../Curve.h"
#include "../../Intercept.h"
#include "../../RasterizerData.h"
#include "../../../Obj/ColorPoint.h"

namespace Rasterization {
    struct RasterizerInterceptStorage {
        std::vector<Intercept> intercepts;
        std::vector<Intercept> frontPadding;
        std::vector<Intercept> backPadding;
        std::vector<ColorPoint> colorPoints;

        void clear() {
            intercepts.clear();
            frontPadding.clear();
            backPadding.clear();
            colorPoints.clear();
        }
    };

    struct RasterizerCurveStorage {
        std::vector<Curve> curves;
        std::vector<GuideCurveRegion> guideCurveRegions;

        void clear() {
            curves.clear();
            guideCurveRegions.clear();
        }
    };

    struct RasterizerWaveformStorage {
        WaveformBuffers waveform { 0, INT_MAX / 2 };

        void nullify() {
            waveform.nullify();
        }
    };

    struct RasterizerSnapshotStorage {
        RasterizerData rasterizerData;
    };

    struct RasterizerStorage {
        RasterizerInterceptStorage intercepts;
        RasterizerCurveStorage curves;
        RasterizerWaveformStorage waveform;
        RasterizerSnapshotStorage snapshot;

        void clear() {
            intercepts.clear();
            curves.clear();
            waveform.nullify();
        }
    };
}
