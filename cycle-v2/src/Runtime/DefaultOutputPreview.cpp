#include "Runtime/DefaultOutputPreview.h"

#include <Array/Buffer.h>

#include "Nodes/FFT/FftGridwiseDsp.h"

namespace CycleV2 {

namespace {

void normalizeContrast(std::vector<float>& values, float targetPeak) {
    if (values.empty()) {
        return;
    }
    std::vector<float> magnitude = values;
    Buffer<float> magnitudeBuffer(magnitude.data(), (int) magnitude.size());
    magnitudeBuffer.abs();
    float peak {};
    int peakIndex {};
    magnitudeBuffer.getMax(peak, peakIndex);
    if (peak > 0.f) {
        Buffer<float>(values.data(), (int) values.size()).mul(targetPeak / peak);
    }
}

}

GraphPreviewResult::SignalProbePreview DefaultOutputPreview::normalizedTime(
        const GraphPreviewResult::SignalProbePreview& preview) {
    auto result = preview;
    normalizeContrast(result.values, 3.f);
    return result;
}

GraphPreviewResult::SignalProbePreview DefaultOutputPreview::spectrum(
        const GraphPreviewResult::SignalProbePreview& preview) {
    auto result = preview;
    result.domain = PortDomain::SpectralMagnitudeSignal;
    result.frequencySampling = TraversalGridFrequencySampling::LinearBins;
    result.values.clear();
    if (!preview.connected || preview.gridColumns == 0 || preview.gridRows == 0
            || preview.values.size() < preview.gridColumns * preview.gridRows) {
        result.connected = false;
        result.gridColumns = 0;
        result.gridRows = 0;
        return result;
    }

    std::vector<AudioProcessBlock> timeColumns;
    timeColumns.reserve(preview.gridColumns);
    for (size_t column = 0; column < preview.gridColumns; ++column) {
        const float* first = preview.values.data() + column * preview.gridRows;
        AudioProcessBlock block;
        block.samples.assign(first, first + preview.gridRows);
        timeColumns.push_back(std::move(block));
    }

    const auto columns = FftGridwiseDsp().forwardColumns(timeColumns);
    if (columns.empty()) {
        result.connected = false;
        return result;
    }
    result.gridRows = columns.front().magnitude.block.samples.size();
    result.values.reserve(result.gridColumns * result.gridRows);
    for (const auto& column : columns) {
        result.values.insert(
                result.values.end(),
                column.magnitude.block.samples.begin(),
                column.magnitude.block.samples.end());
    }
    normalizeContrast(result.values, 1.f);
    return result;
}

}
