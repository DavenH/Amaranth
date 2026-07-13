#pragma once

#include <Array/Buffer.h>
#include <App/Transforms.h>

namespace CycleDsp {

int irImpulseLength(double normalizedValue);
double irImpulseLengthValue(int length);
float irPostGain(double normalizedValue);
float irPrefilterAmount(double normalizedValue);

void buildIrPrefilterLevels(Buffer<float> levels, double normalizedValue);
void applyIrFrequencyPrefilter(
        Buffer<float> rawImpulse,
        Buffer<float> filteredImpulse,
        Buffer<float> levels,
        Transform& transform);

}
