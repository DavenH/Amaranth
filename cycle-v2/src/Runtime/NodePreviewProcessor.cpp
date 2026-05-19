#include "NodePreviewProcessor.h"

namespace CycleV2 {

namespace {

void ensurePreview(PreviewProcessContext& context) {
    context.primary.resize(context.pointCount);
    context.secondary.resize(context.pointCount);
}

float parameterFloat(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        float fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value.getFloatValue();
        }
    }

    return fallback;
}

class FixedPreviewProcessor final : public NodePreviewProcessor {
public:
    explicit FixedPreviewProcessor(PreviewModuleRole roleToUse) : previewRole(roleToUse) {}

    PreviewModuleRole role() const override { return previewRole; }

    void render(PreviewProcessContext& context) override {
        switch (previewRole) {
            case PreviewModuleRole::Waveform:
                renderWaveform(context);
                break;

            case PreviewModuleRole::Image:
                renderImage(context);
                break;

            case PreviewModuleRole::MeshSurface:
                renderMeshSurface(context);
                break;

            case PreviewModuleRole::SpectrumMagnitude:
                renderDescending(context, 1.f, 0.1f);
                break;

            case PreviewModuleRole::SpectrumPhase:
                renderAlternating(context);
                break;

            case PreviewModuleRole::Envelope:
                renderEnvelope(context);
                break;

            case PreviewModuleRole::ImpulseResponse:
                renderImpulse(context);
                break;

            case PreviewModuleRole::Waveshaper:
                renderTransfer(context);
                break;

            case PreviewModuleRole::OutputMeters:
                renderMeters(context);
                break;

            case PreviewModuleRole::VoiceContext:
            case PreviewModuleRole::Generic:
                renderDescending(context, 0.8f, 0.2f);
                break;

            case PreviewModuleRole::None:
            default:
                context.primary.clear();
                context.secondary.clear();
                break;
        }
    }

private:
    void renderWaveform(PreviewProcessContext& context) const {
        ensurePreview(context);

        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1 ? (float) (context.pointCount - 1) : 1.f;
        const float amplitude = parameterFloat(context.parameters, "amplitude", 1.f);

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float phase = (float) i / denominator;
            context.primary[i] = (phase <= 0.5f ? phase * 2.f : (1.f - phase) * 2.f) * amplitude;
            context.secondary[i] = 1.f - context.primary[i];
        }
    }

    void renderImage(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = (float) (i % 4) / 3.f;
            context.secondary[i] = (float) ((i + 2) % 4) / 3.f;
        }
    }

    void renderMeshSurface(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = (i % 2) == 0 ? 0.25f : 0.75f;
            context.secondary[i] = (float) (i % 5) / 4.f;
        }
    }

    void renderDescending(PreviewProcessContext& context, float top, float bottom) const {
        ensurePreview(context);

        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1 ? (float) (context.pointCount - 1) : 1.f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float phase = (float) i / denominator;
            context.primary[i] = top + (bottom - top) * phase;
            context.secondary[i] = 0.f;
        }
    }

    void renderAlternating(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = (i % 2) == 0 ? 0.2f : 0.8f;
            context.secondary[i] = 0.5f;
        }
    }

    void renderEnvelope(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            const bool attack = i < context.pointCount / 4;
            const bool sustain = i < context.pointCount / 2;
            context.primary[i] = attack ? 1.f : (sustain ? 0.8f : 0.3f);
            context.secondary[i] = sustain ? 0.4f : 0.f;
        }
    }

    void renderImpulse(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = i == 0 ? 1.f : 1.f / (float) (i + 1);
            context.secondary[i] = 0.f;
        }
    }

    void renderTransfer(PreviewProcessContext& context) const {
        ensurePreview(context);

        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1 ? (float) (context.pointCount - 1) : 1.f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            const float x = (float) i / denominator;
            context.primary[i] = x * x;
            context.secondary[i] = x;
        }
    }

    void renderMeters(PreviewProcessContext& context) const {
        ensurePreview(context);

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = 0.65f;
            context.secondary[i] = 0.62f;
        }
    }

    PreviewModuleRole previewRole {};
};

}

std::unique_ptr<NodePreviewProcessor> NodePreviewProcessorFactory::create(PreviewModuleRole role) const {
    if (role == PreviewModuleRole::None) {
        return {};
    }

    return std::make_unique<FixedPreviewProcessor>(role);
}

}
