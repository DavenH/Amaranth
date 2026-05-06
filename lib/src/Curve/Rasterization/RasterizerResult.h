#pragma once

#include <vector>

#include "RasterizerTypes.h"
#include "../Curve.h"
#include "../Intercept.h"
#include "../../Array/Buffer.h"
#include "../../Obj/ColorPoint.h"

namespace Rasterization {
    struct InterceptBuildResult {
        std::vector<RasterPoint> points;
        std::vector<ColorPoint> colorPoints;
        bool needsResort {};
    };

    struct CurveBuildResult {
        std::vector<Curve> curves;
        std::vector<Intercept> frontPadding;
        std::vector<Intercept> backPadding;
        int paddingSize {};
    };

    struct WaveformResult {
        Buffer<float> waveX;
        Buffer<float> waveY;
        Buffer<float> diffX;
        Buffer<float> slope;
        Buffer<float> area;
        int zeroIndex {};
        int oneIndex {};
        bool sampleable {};
    };
}
