#include "SpectralPreviewMapping.h"

#include <JuceHeader.h>

#include <Array/Buffer.h>
#include <Util/Arithmetic.h>

#include <algorithm>
#include <limits>

namespace CycleV2::SpectralPreviewMapping {

namespace {

std::vector<float> rowsWithoutDc(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) {
    if (columns == 0 || rows < 2 || source.size() < columns * rows) {
        return source;
    }

    std::vector<float> surface(source.size());
    std::vector<float> sourceRows(rows);
    Buffer<float> sourceRowPositions(sourceRows.data(), (int) sourceRows.size());
    sourceRowPositions.ramp(0.f, 1.f / (float) (rows - 1));
    Arithmetic::applyInvLogMapping(sourceRowPositions, 500.f);
    sourceRowPositions.mul((float) (rows - 2)).add(1.f);

    for (size_t column = 0; column < columns; ++column) {
        const size_t columnOffset = column * rows;

        for (size_t row = 0; row < rows; ++row) {
            const float position = jlimit(1.f, (float) (rows - 1), sourceRows[row]);
            const size_t rowA = (size_t) position;
            const size_t rowB = std::min(rowA + 1, rows - 1);
            const float amount = position - (float) rowA;

            surface[columnOffset + row] = source[columnOffset + rowA]
                    + amount * (source[columnOffset + rowB] - source[columnOffset + rowA]);
        }
    }

    return surface;
}

}

std::vector<float> magnitudeSurface(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) {
    std::vector<float> surface = rowsWithoutDc(source, columns, rows);

    if (!surface.empty()) {
        Buffer<float>(surface.data(), (int) surface.size())
                .abs()
                .mul(16.f)
                .add(1.f)
                .ln()
                .mul(1.f / 2.833213344f)
                .clip(0.f, 1.f);
    }

    return surface;
}

std::vector<float> phaseSurface(
        const std::vector<float>& source,
        size_t columns,
        size_t rows) {
    std::vector<float> surface = rowsWithoutDc(source, columns, rows);
    if (surface.empty()) {
        return surface;
    }

    Buffer<float> buffer(surface.data(), (int) surface.size());
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();
    int minimumIndex {};
    int maximumIndex {};
    buffer.getMin(minimum, minimumIndex);
    buffer.getMax(maximum, maximumIndex);
    const float realMaximum = jmax(std::abs(minimum), std::abs(maximum));

    if (realMaximum <= 0.f) {
        buffer.set(0.5f);
    } else {
        const float exponent = std::ceil(std::log2(realMaximum) + 0.5f);
        buffer.mul(std::pow(2.f, -exponent)).add(0.5f).clip(0.f, 1.f);
    }

    return surface;
}

}
