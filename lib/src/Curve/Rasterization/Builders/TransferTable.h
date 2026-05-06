#pragma once

#include <cmath>

#include "../../Curve.h"

namespace Rasterization {
    class TransferTable {
    public:
        static const float* values() {
            static bool alreadyCalculated = false;
            static float table[Curve::resolution];

            if (!alreadyCalculated) {
                const float pi = MathConstants<float>::pi;
                double invSize = 1.0 / double(Curve::resolution);

                for (int i = 0; i < Curve::resolution; ++i) {
                    double x = i * invSize;
                    table[i] = (float) x
                             - 0.2180285f * sinf(2.f * pi * (float) x)
                             + 0.0322599f * sinf(4.f * pi * (float) x)
                             - 0.0018794f * sinf(6.f * pi * (float) x);
                }

                alreadyCalculated = true;
            }

            return table;
        }
    };
}
