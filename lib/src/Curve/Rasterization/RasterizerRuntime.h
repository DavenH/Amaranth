#pragma once

#include <vector>

#include "WaveformBuffers.h"
#include "../Curve.h"
#include "../Intercept.h"
#include "../../Obj/ColorPoint.h"

namespace Rasterization {
    struct RasterizerRuntime {
        std::vector<Intercept>* intercepts {};
        std::vector<Curve>* curves {};
        std::vector<Intercept>* frontPadding {};
        std::vector<Intercept>* backPadding {};
        std::vector<ColorPoint>* colorPoints {};

        WaveformBufferRefs waveform;

        int* paddingSize {};
        bool* unsampleable {};
        bool* needsResorting {};

        bool isBound() const {
            return intercepts != nullptr
                && curves != nullptr
                && waveform.isBound()
                && paddingSize != nullptr
                && unsampleable != nullptr;
        }

        bool isSampleable() const {
            return unsampleable != nullptr && !*unsampleable;
        }
    };
}
