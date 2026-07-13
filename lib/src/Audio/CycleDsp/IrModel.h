#pragma once

#include <Array/Buffer.h>
#include <Algo/Oversampler.h>
#include <App/Transforms.h>
#include <Curve/Rasterization/Rasterizer/RasterizerViews.h>

namespace CycleDsp {

int irImpulseLength(double normalizedValue);
double irImpulseLengthValue(int length);
float irPostGain(double normalizedValue);
float irPrefilterAmount(double normalizedValue);

void buildIrPrefilterLevels(Buffer<float> levels, double normalizedValue);
void rasterizeIrImpulse(
        Rasterization::SamplerView sampler,
        Buffer<float> impulse,
        Oversampler& oversampler,
        double padding);
void applyIrFrequencyPrefilter(
        Buffer<float> rawImpulse,
        Buffer<float> filteredImpulse,
        Buffer<float> levels,
        Transform& transform);

}
