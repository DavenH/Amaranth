#include "PreviewProcessorFactories.h"

namespace CycleV2 {

namespace {

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

}
