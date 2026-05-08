#pragma once

#include "../../../Array/Buffer.h"
#include "../../../Design/Updating/Updateable.h"

namespace Rasterization {
    class WaveformProvider {
    public:
        virtual ~WaveformProvider() = default;

        virtual bool canRasterizeWaveform() const = 0;
        virtual bool isBipolar() const = 0;
        virtual void updateWaveform(UpdateType updateType) = 0;
        virtual double sampleWithInterval(Buffer<float> buffer, double delta, double phase) = 0;
        virtual float samplePerfectly(double delta, Buffer<float> buffer, double phase) = 0;
    };
}
