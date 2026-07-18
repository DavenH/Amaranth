#include "PreviewProcessorFactories.h"

#include "../Nodes/Effects/EffectSignalProcessors.h"

#include <Audio/CycleDsp/EqualizerCore.h>

#include <cmath>

namespace CycleV2 {

namespace {

class EqualizerPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::EqualizerResponse; }

    void render(PreviewProcessContext& context) override {
        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const EqualizerConfiguration>(
                        context.configuration->value)
                : nullptr;
        if (configuration == nullptr || context.pointCount == 0) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        CycleDsp::EqualizerCore core(1);
        for (int band = 0; band < CycleDsp::equalizerBandCount; ++band) {
            core.configureBand(
                    band,
                    44100.0,
                    configuration->frequencies[(size_t) band],
                    configuration->gains[(size_t) band]);
        }

        context.primary.resize(context.pointCount);
        context.secondary.assign(context.pointCount, 0.5f);
        const double denominator = context.pointCount > 1
                ? (double) context.pointCount - 1.0
                : 1.0;
        for (size_t index = 0; index < context.pointCount; ++index) {
            const double unit = (double) index / denominator;
            const double frequency = 40.0 * std::pow(400.0, unit);
            context.primary[index] = jlimit(
                    0.f,
                    1.f,
                    core.responseDecibels(frequency) / 60.f + 0.5f);
        }
        context.domain = PortDomain::TimeSignal;
    }
};

void ensurePreview(PreviewProcessContext& context) {
    context.primary.resize(context.pointCount);
    context.secondary.resize(context.pointCount);
}

class EnvelopePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Envelope; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            const bool attack = i < context.pointCount / 4;
            const bool sustain = i < context.pointCount / 2;
            context.primary[i] = attack ? 1.f : (sustain ? 0.8f : 0.3f);
            context.secondary[i] = sustain ? 0.4f : 0.f;
        }
    }
};

class ImpulsePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::ImpulseResponse; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = i == 0 ? 1.f : 1.f / (float) (i + 1);
            context.secondary[i] = 0.f;
        }
    }
};

class TransferPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Waveshaper; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);
        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1
                ? (float) (context.pointCount - 1)
                : 1.f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float x = (float) i / denominator;
            context.primary[i] = x * x;
            context.secondary[i] = x;
        }
    }
};

}

std::unique_ptr<NodePreviewProcessor> createEnvelopePreviewProcessor() {
    return std::make_unique<EnvelopePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createImpulseResponsePreviewProcessor() {
    return std::make_unique<ImpulsePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createWaveshaperPreviewProcessor() {
    return std::make_unique<TransferPreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createEqualizerPreviewProcessor() {
    return std::make_unique<EqualizerPreviewProcessor>();
}

}
