#include "NodePreviewProcessor.h"

#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshMeshFactory.h"

#include <Array/Buffer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

#include <algorithm>

namespace CycleV2 {

namespace {

void ensurePreview(PreviewProcessContext& context) {
    context.primary.resize(context.pointCount);
    context.secondary.resize(context.pointCount);
}

float averageSummary(const std::vector<float>& summary) {
    if (summary.empty()) {
        return 0.f;
    }

    float total = 0.f;

    for (const float value : summary) {
        total += value;
    }

    return total / (float) summary.size();
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

String parameterString(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        const String& fallback) {
    for (const auto& parameter : parameters) {
        if (parameter.id == id) {
            return parameter.value;
        }
    }

    return fallback;
}

int primaryAxisFromParameter(const String& axisName) {
    if (axisName == "red") {
        return Vertex::Red;
    }

    if (axisName == "blue") {
        return Vertex::Blue;
    }

    return Vertex::Time;
}

MorphPosition meshMorphFromParameters(const std::vector<NodeParameter>& parameters) {
    return {
            parameterFloat(parameters, "yellow", 0.5f),
            parameterFloat(parameters, "red", 0.5f),
            parameterFloat(parameters, "blue", 0.5f)
    };
}

PortDomain primaryOutputDomain(const std::vector<PreviewOutputPort>& outputPorts) {
    if (outputPorts.empty()) {
        return PortDomain::TimeSignal;
    }

    return outputPorts.front().domain;
}

void normalizeBipolarBlock(AudioProcessBlock& block) {
    if (block.samples.empty()) {
        return;
    }

    Buffer<float>(block.samples.data(), (int) block.samples.size())
            .mul(0.5f)
            .add(0.5f)
            .clip(0.f, 1.f);
}

class FixedPreviewProcessor final : public NodePreviewProcessor {
public:
    explicit FixedPreviewProcessor(PreviewModuleRole roleToUse) : previewRole(roleToUse) {}

    ~FixedPreviewProcessor() override {
        if (defaultMesh != nullptr) {
            defaultMesh->destroy();
        }
    }

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
        if (context.pointCount == 0) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        Mesh& mesh = meshForPreview();
        const MorphPosition morph = meshMorphFromParameters(context.parameters);
        const int primaryAxis = primaryAxisFromParameter(
                parameterString(context.parameters, "primaryAxis", "yellow"));
        const PortDomain outputDomain = primaryOutputDomain(context.outputPorts);
        const bool cyclic = outputDomain == PortDomain::TimeSignal;
        const size_t columnCount = std::max<size_t>(8, context.pointCount / 2);

        TrimeshBlockwiseDsp blockwiseDsp;
        AudioProcessBlock slice;
        blockwiseDsp.setMesh(&mesh);
        blockwiseDsp.setMorphPosition(morph);
        blockwiseDsp.setPrimaryViewAxis(primaryAxis);
        blockwiseDsp.setCyclic(cyclic);
        blockwiseDsp.renderCycle(
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo,
                slice);
        normalizeBipolarBlock(slice);
        context.secondary = std::move(slice.samples);

        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(cyclic);
        const auto columns = gridwiseDsp.renderColumns(
                mesh,
                morph,
                primaryAxis,
                columnCount,
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo);

        context.primary.clear();
        context.primary.reserve(columnCount * context.pointCount);

        for (auto column : columns) {
            normalizeBipolarBlock(column.signal);
            context.primary.insert(
                    context.primary.end(),
                    column.signal.samples.begin(),
                    column.signal.samples.end());
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
        const bool hasInput = !context.inputSummary.empty();
        const float level = hasInput ? averageSummary(context.inputSummary) : 0.65f;
        const float secondaryLevel = hasInput ? level * 0.95f : 0.62f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = level;
            context.secondary[i] = secondaryLevel;
        }
    }

    Mesh& meshForPreview() const {
        if (defaultMesh == nullptr) {
            defaultMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2PreviewMesh");
        }

        return *defaultMesh;
    }

    PreviewModuleRole previewRole {};
    mutable std::unique_ptr<Mesh> defaultMesh;
};

}

std::unique_ptr<NodePreviewProcessor> NodePreviewProcessorFactory::create(PreviewModuleRole role) const {
    if (role == PreviewModuleRole::None) {
        return {};
    }

    return std::make_unique<FixedPreviewProcessor>(role);
}

}
