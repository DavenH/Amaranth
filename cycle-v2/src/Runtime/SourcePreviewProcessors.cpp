#include "PreviewProcessorFactories.h"

namespace CycleV2 {

namespace {

void ensurePreview(PreviewProcessContext& context) {
    context.primary.resize(context.pointCount);
    context.secondary.resize(context.pointCount);
}

class WaveformPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Waveform; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);
        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1
                ? (float) (context.pointCount - 1)
                : 1.f;
        const float amplitude = typedParameterFloat(context.parameters, "amplitude", 1.f);

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float phase = (float) i / denominator;
            context.primary[i] = (phase <= 0.5f
                    ? phase * 2.f
                    : (1.f - phase) * 2.f) * amplitude;
            context.secondary[i] = 1.f - context.primary[i];
        }
    }
};

class ImagePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::Image; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = (float) (i % 4) / 3.f;
            context.secondary[i] = (float) ((i + 2) % 4) / 3.f;
        }
    }
};

class DescendingPreviewProcessor final : public NodePreviewProcessor {
public:
    DescendingPreviewProcessor(PreviewModuleRole role, float top, float bottom) :
            top(top), bottom(bottom), previewRole(role) {}

    PreviewModuleRole role() const override { return previewRole; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);
        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1
                ? (float) (context.pointCount - 1)
                : 1.f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float phase = (float) i / denominator;
            context.primary[i] = top + (bottom - top) * phase;
            context.secondary[i] = 0.f;
        }
    }

private:
    float top {};
    float bottom {};
    PreviewModuleRole previewRole {};
};

class PhasePreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::SpectrumPhase; }

    void render(PreviewProcessContext& context) override {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = (i % 2) == 0 ? 0.2f : 0.8f;
            context.secondary[i] = 0.5f;
        }
    }
};

}

std::unique_ptr<NodePreviewProcessor> createWaveformPreviewProcessor() {
    return std::make_unique<WaveformPreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createImagePreviewProcessor() {
    return std::make_unique<ImagePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createSpectrumMagnitudePreviewProcessor() {
    return std::make_unique<DescendingPreviewProcessor>(
            PreviewModuleRole::SpectrumMagnitude,
            1.f,
            0.1f);
}

std::unique_ptr<NodePreviewProcessor> createSpectrumPhasePreviewProcessor() {
    return std::make_unique<PhasePreviewProcessor>();
}

std::unique_ptr<NodePreviewProcessor> createVoiceContextPreviewProcessor() {
    return std::make_unique<DescendingPreviewProcessor>(
            PreviewModuleRole::VoiceContext,
            0.8f,
            0.2f);
}

std::unique_ptr<NodePreviewProcessor> createGenericPreviewProcessor() {
    return std::make_unique<DescendingPreviewProcessor>(
            PreviewModuleRole::Generic,
            0.8f,
            0.2f);
}

}
