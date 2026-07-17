#include "NodePreviewProcessor.h"

#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../Nodes/Trimesh/PreparedTrimeshTopology.h"

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
            typedParameterFloat(parameters, "yellow", 0.5f),
            typedParameterFloat(parameters, "red", 0.5f),
            typedParameterFloat(parameters, "blue", 0.5f)
    };
}

PortDomain primaryOutputDomain(const std::vector<PreviewOutputPort>& outputPorts) {
    if (outputPorts.empty()) {
        return PortDomain::TimeSignal;
    }

    return outputPorts.front().domain;
}

void normalizeBipolarBlock(SignalPayload& payload) {
    if (payload.block.samples.empty()) {
        return;
    }

    Buffer<float>(payload.block.samples.data(), (int) payload.block.samples.size())
            .mul(0.5f)
            .add(0.5f)
            .clip(0.f, 1.f);
}

void normalizeBipolarBlockValues(std::vector<float>& values) {
    if (values.empty()) {
        return;
    }

    Buffer<float>(values.data(), (int) values.size())
            .mul(0.5f)
            .add(0.5f)
            .clip(0.f, 1.f);
}

class PreviewProcessorBase : public NodePreviewProcessor {
public:
    explicit PreviewProcessorBase(PreviewModuleRole roleToUse) : previewRole(roleToUse) {}

    PreviewModuleRole role() const override { return previewRole; }

protected:
    void renderWaveform(PreviewProcessContext& context) const {
        ensurePreview(context);

        if (context.pointCount == 0) {
            return;
        }

        const float denominator = context.pointCount > 1 ? (float) (context.pointCount - 1) : 1.f;
        const float amplitude = typedParameterFloat(context.parameters, "amplitude", 1.f);

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
        const bool hasInput = context.input.summary != nullptr
                && !context.input.summary->empty();
        const float level = hasInput ? averageSummary(context.input.summary) : 0.65f;
        const float secondaryLevel = hasInput ? level * 0.95f : 0.62f;

        for (size_t i = 0; i < context.pointCount; ++i) {
            context.primary[i] = level;
            context.secondary[i] = secondaryLevel;
        }
    }

    void renderSignalSpy(PreviewProcessContext& context) const {
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

private:
    PreviewModuleRole previewRole {};
};

class WaveformPreviewProcessor final : public PreviewProcessorBase {
public:
    WaveformPreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::Waveform) {}
    void render(PreviewProcessContext& context) override { renderWaveform(context); }
};

class ImagePreviewProcessor final : public PreviewProcessorBase {
public:
    ImagePreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::Image) {}
    void render(PreviewProcessContext& context) override { renderImage(context); }
};

class DescendingPreviewProcessor final : public PreviewProcessorBase {
public:
    DescendingPreviewProcessor(PreviewModuleRole role, float top, float bottom) :
            PreviewProcessorBase(role), top(top), bottom(bottom) {}
    void render(PreviewProcessContext& context) override { renderDescending(context, top, bottom); }

private:
    float top {};
    float bottom {};
};

class PhasePreviewProcessor final : public PreviewProcessorBase {
public:
    PhasePreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::SpectrumPhase) {}
    void render(PreviewProcessContext& context) override { renderAlternating(context); }
};

class EnvelopePreviewProcessor final : public PreviewProcessorBase {
public:
    EnvelopePreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::Envelope) {}
    void render(PreviewProcessContext& context) override { renderEnvelope(context); }
};

class ImpulsePreviewProcessor final : public PreviewProcessorBase {
public:
    ImpulsePreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::ImpulseResponse) {}
    void render(PreviewProcessContext& context) override { renderImpulse(context); }
};

class TransferPreviewProcessor final : public PreviewProcessorBase {
public:
    TransferPreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::Waveshaper) {}
    void render(PreviewProcessContext& context) override { renderTransfer(context); }
};

class MeterPreviewProcessor final : public PreviewProcessorBase {
public:
    MeterPreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::OutputMeters) {}
    void render(PreviewProcessContext& context) override { renderMeters(context); }
};

class SignalSpyPreviewProcessor final : public PreviewProcessorBase {
public:
    SignalSpyPreviewProcessor() : PreviewProcessorBase(PreviewModuleRole::SignalSpy) {}
    void render(PreviewProcessContext& context) override { renderSignalSpy(context); }
};

class TrimeshPreviewProcessor final : public NodePreviewProcessor {
public:
    PreviewModuleRole role() const override { return PreviewModuleRole::MeshSurface; }

    void render(PreviewProcessContext& context) override {
        if (context.pointCount == 0) {
            context.primary.clear();
            context.secondary.clear();
            return;
        }

        if (context.capturedOutput != nullptr
                && context.capturedOutput->traversalGrid.isValid()
                && context.capturedOutput->traversalGrid.rows == context.pointCount) {
            context.primary = context.capturedOutput->traversalGrid.values;
            context.secondary = context.capturedOutput->block.samples;
            normalizeBipolarBlockValues(context.primary);
            normalizeBipolarBlockValues(context.secondary);
            context.gridColumns = context.capturedOutput->traversalGrid.columns;
            context.gridRows = context.capturedOutput->traversalGrid.rows;
            context.domain = context.capturedOutput->traversalGrid.metadata.valueDomain;
            context.reusedCapturedTraversal = true;
            return;
        }

        const auto configuration = context.configuration != nullptr
                ? std::dynamic_pointer_cast<const TrimeshConfiguration>(context.configuration->value)
                : nullptr;
        Mesh* preparedMesh = configuration != nullptr
                ? configuration->mesh.get()
                : &fallbackTopology.mesh(context.parameters);
        const MorphPosition morph = configuration != nullptr
                ? configuration->morph
                : meshMorphFromParameters(context.parameters);
        const int primaryAxis = configuration != nullptr
                ? configuration->primaryViewAxis
                : primaryAxisFromParameter(typedParameterString(
                        context.parameters, "primaryAxis", "yellow"));
        const PortDomain outputDomain = primaryOutputDomain(context.outputPorts);
        const bool cyclic = outputDomain == PortDomain::TimeSignal;
        const size_t columnCount = std::max<size_t>(8, context.pointCount / 2);
        context.domain = outputDomain;

        TrimeshBlockwiseDsp blockwiseDsp;
        SignalPayload slice;
        blockwiseDsp.setMesh(preparedMesh);
        blockwiseDsp.setMorphPosition(morph);
        blockwiseDsp.setPrimaryViewAxis(primaryAxis);
        blockwiseDsp.setCyclic(cyclic);
        blockwiseDsp.renderCycle(
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo,
                slice);
        normalizeBipolarBlock(slice);
        context.secondary = std::move(slice.block.samples);

        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(cyclic);
        const auto columns = gridwiseDsp.renderColumns(
                *preparedMesh,
                morph,
                primaryAxis,
                columnCount,
                context.pointCount,
                outputDomain,
                ChannelLayout::LinkedStereo);
        context.primary.clear();
        context.primary.reserve(columnCount * context.pointCount);
        context.gridColumns = columnCount;
        context.gridRows = context.pointCount;
        for (auto column : columns) {
            normalizeBipolarBlock(column.signal);
            context.primary.insert(
                    context.primary.end(),
                    column.signal.block.samples.begin(),
                    column.signal.block.samples.end());
        }
    }

private:
    PreparedTrimeshTopology fallbackTopology { "CycleV2PreviewMesh" };
};

}

std::unique_ptr<NodePreviewProcessor> NodePreviewProcessorFactory::create(PreviewModuleRole role) const {
    using Factory = std::unique_ptr<NodePreviewProcessor> (*)();
    struct Registration {
        PreviewModuleRole role;
        Factory factory;
    };
    static const Registration registrations[] {
            { PreviewModuleRole::Waveform, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<WaveformPreviewProcessor>(); } },
            { PreviewModuleRole::Image, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<ImagePreviewProcessor>(); } },
            { PreviewModuleRole::MeshSurface, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<TrimeshPreviewProcessor>(); } },
            { PreviewModuleRole::SpectrumMagnitude, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<DescendingPreviewProcessor>(PreviewModuleRole::SpectrumMagnitude, 1.f, 0.1f); } },
            { PreviewModuleRole::SpectrumPhase, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<PhasePreviewProcessor>(); } },
            { PreviewModuleRole::Envelope, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<EnvelopePreviewProcessor>(); } },
            { PreviewModuleRole::ImpulseResponse, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<ImpulsePreviewProcessor>(); } },
            { PreviewModuleRole::Waveshaper, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<TransferPreviewProcessor>(); } },
            { PreviewModuleRole::OutputMeters, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<MeterPreviewProcessor>(); } },
            { PreviewModuleRole::SignalSpy, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<SignalSpyPreviewProcessor>(); } },
            { PreviewModuleRole::VoiceContext, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<DescendingPreviewProcessor>(PreviewModuleRole::VoiceContext, 0.8f, 0.2f); } },
            { PreviewModuleRole::Generic, []() -> std::unique_ptr<NodePreviewProcessor> { return std::make_unique<DescendingPreviewProcessor>(PreviewModuleRole::Generic, 0.8f, 0.2f); } }
    };

    for (const auto& registration : registrations) {
        if (registration.role == role) {
            return registration.factory();
        }
    }

    return {};
}

}
