#include "PreviewProcessorFactories.h"

namespace CycleV2 {

namespace {

float averageSummary(const std::vector<float>* summary) {
    if (summary == nullptr || summary->empty()) {
        return 0.f;
    }

    float total = 0.f;
    for (const float value : *summary) {
        total += value;
    }

    return total / (float) summary->size();
}

class MeterPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::OutputMeters; }

    void render(PreviewProcessContext& context) override {
        context.primary.resize(context.pointCount);
        context.secondary.resize(context.pointCount);

        const bool hasInput = context.input.summary != nullptr
                && !context.input.summary->empty();
        const float level = hasInput ? averageSummary(context.input.summary) : 0.65f;
        const float secondaryLevel = hasInput ? level * 0.95f : 0.62f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = level;
            context.secondary[i] = secondaryLevel;
        }
    }
};

class SignalSpyPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::SignalSpy; }

    void render(PreviewProcessContext& context) override {
        if (context.input.grid == nullptr || context.input.grid->empty()) {
            context.primary.clear();
            context.secondary.clear();
            context.gridColumns = 0;
            context.gridRows = 0;
            return;
        }

        context.primary.assign(context.input.grid->begin(), context.input.grid->end());
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
