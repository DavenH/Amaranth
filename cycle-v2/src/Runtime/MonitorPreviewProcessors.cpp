#include <algorithm>
#include <cmath>

#include "Runtime/PreviewProcessorFactories.h"

namespace CycleV2 {

namespace {

float peakLevel(const SignalBlock& block) {
    if (block.samples.empty()) {
        return 0.f;
    }

    const auto extrema = std::minmax_element(block.samples.begin(), block.samples.end());
    const float minimumMagnitude = *extrema.first < 0.f
            ? -*extrema.first
            : *extrema.first;
    const float maximumMagnitude = *extrema.second < 0.f
            ? -*extrema.second
            : *extrema.second;
    const float peak = jmax(minimumMagnitude, maximumMagnitude);
    return std::isfinite(peak) ? jlimit(0.f, 1.f, peak) : 0.f;
}

class MeterPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::OutputMeters; }

    void render(PreviewProcessContext& context) override {
        const SignalPayload* output = context.capturedOutput;
        const float left = output != nullptr ? peakLevel(output->block) : 0.f;
        const float right = output != nullptr && output->isStereo()
                ? peakLevel(output->secondaryBlock)
                : left;

        context.primary.assign(context.pointCount, left);
        context.secondary.assign(context.pointCount, right);
    }
};

class SignalSpyPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::SignalSpy; }

    void render(PreviewProcessContext& context) override {
        if (context.input.grid == nullptr || context.input.gridSize == 0) {
            context.primary.clear();
            context.secondary.clear();
            context.gridColumns = 0;
            context.gridRows = 0;
            return;
        }

        context.primary.assign(context.input.grid, context.input.grid + context.input.gridSize);
        context.secondary.clear();
        context.gridColumns = context.input.gridColumns;
        context.gridRows = context.input.gridRows;
    }
};

}

std::unique_ptr<NodePreviewProcessor> createOutputMetersPreviewProcessor() {
    return std::make_unique<MeterPreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createSignalSpyPreviewProcessor() {
    return std::make_unique<SignalSpyPreviewProcessor>();
}

}
