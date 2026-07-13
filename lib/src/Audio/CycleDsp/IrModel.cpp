#include "IrModel.h"

#include <cmath>

namespace CycleDsp {

int irImpulseLength(double normalizedValue) {
    return 1 << int(7 + normalizedValue * 7);
}

double irImpulseLengthValue(int length) {
    return (std::log(length) / std::log(2.0) - 7.) / 7.;
}

float irPostGain(double normalizedValue) {
    return (float) std::exp(10. * normalizedValue - 5.);
}

float irPrefilterAmount(double normalizedValue) {
    return (float) (normalizedValue * normalizedValue * normalizedValue);
}

void buildIrPrefilterLevels(Buffer<float> levels, double normalizedValue) {
    levels.set(1.f).zero((int) (irPrefilterAmount(normalizedValue) * levels.size()));
}

void applyIrFrequencyPrefilter(
        Buffer<float> rawImpulse,
        Buffer<float> filteredImpulse,
        Buffer<float> levels,
        Transform& transform) {
    transform.forward(rawImpulse);
    transform.getMagnitudes().mul(levels);
    transform.inverse(filteredImpulse);
}

}
