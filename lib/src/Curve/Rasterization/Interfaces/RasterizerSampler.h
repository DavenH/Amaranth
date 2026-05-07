#pragma once

#include "../../../Array/Buffer.h"

namespace Rasterization {
    class RasterizerSampler {
    public:
        virtual ~RasterizerSampler() = default;

        virtual bool isSampleable() = 0;
        virtual float sampleAt(double angle) = 0;
        virtual float sampleAt(double angle, int& currentIndex) = 0;
        virtual float samplePerfectly(double delta, Buffer<float> buffer, double phase) = 0;
    };
}
