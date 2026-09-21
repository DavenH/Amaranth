#include <Array/Buffer.h>

#include "Runtime/PreviewContrastNormalization.h"

namespace CycleV2 {

namespace {

float peakMagnitude(const std::vector<float>& values) {
    if (values.empty()) {
        return 0.f;
    }

    std::vector<float> magnitude = values;
    Buffer<float> magnitudeBuffer(magnitude.data(), (int) magnitude.size());
    magnitudeBuffer.abs();
    float peak {};
    int peakIndex {};
    magnitudeBuffer.getMax(peak, peakIndex);
    return peak;
}

void scaleToPeak(std::vector<float>& values, float peak, float targetPeak) {
    if (peak > 0.f) {
        Buffer<float>(values.data(), (int) values.size()).mul(targetPeak / peak);
    }
}

}

void PreviewContrastNormalization::apply(
        std::vector<float>& values,
        float targetPeak) {
    scaleToPeak(values, peakMagnitude(values), targetPeak);
}

void PreviewContrastNormalization::applySpectralMagnitude(
        std::vector<float>& values,
        size_t columns,
        size_t rows,
        float targetPeak) {
    if (columns == 0 || rows < 2 || values.size() < columns * rows) {
        return;
    }

    std::vector<float> visibleValues = values;
    for (size_t column = 0; column < columns; ++column) {
        visibleValues[column * rows] = 0.f;
    }
    scaleToPeak(values, peakMagnitude(visibleValues), targetPeak);
}

}
