#include <Array/Buffer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Rasterization/ScratchPositionPolicy.h>
#include <Obj/MorphPosition.h>

#include "AudioProcessContextUtils.h"
#include "AudioProcessorFactories.h"
#include "SmoothedMorphPosition.h"

#include "../Nodes/Trimesh/PreparedTrimeshTopology.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"

namespace CycleV2 {

namespace {

constexpr size_t kDefaultTraversalColumns = 8;

size_t traversalRowsForDomain(PortDomain domain, size_t frameCount) {
    if ((domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal)
            && frameCount > 1) {
        return frameCount / 2 + 1;
    }

    return frameCount;
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

Rasterization::ScratchSourceDomain scratchDomainFor(PortDomain domain) {
    if (domain == PortDomain::TimeSignal) {
        return Rasterization::ScratchSourceDomain::Time;
    }
    if (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        return Rasterization::ScratchSourceDomain::Spectral;
    }
    return Rasterization::ScratchSourceDomain::Unsupported;
}

const SignalPayload* scratchAttachment(const AudioProcessContext& context) {
    for (const auto& attachment : context.attachments) {
        if (attachment.destPortId == "scratch" && attachment.payload != nullptr) {
            return attachment.payload;
        }
    }
    return nullptr;
}

float scratchCoordinateForColumn(
        const SignalPayload& scratch,
        size_t column,
        size_t columnCount,
        float fallback) {
    if (scratch.traversalGrid.isValid()) {
        const size_t sourceColumn = std::min(
                scratch.traversalGrid.columns - 1,
                column * scratch.traversalGrid.columns / columnCount);
        return scratch.traversalGrid.values[
                sourceColumn * scratch.traversalGrid.rows];
    }
    if (!scratch.block.samples.empty()) {
        const size_t sourceSample = std::min(
                scratch.block.samples.size() - 1,
                column * scratch.block.samples.size() / columnCount);
        return scratch.block.samples[sourceSample];
    }
    return fallback;
}

class TrimeshAudioProcessor final : public NodeAudioProcessor {
public:
    explicit TrimeshAudioProcessor(AudioModuleRole processorRoleToUse) :
            processorRole(processorRoleToUse) {
    }

    AudioModuleRole role() const override { return processorRole; }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(published.value);
    }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        trimeshDsp.prepareSampling(spec.maximumFrameCount);
        trimeshGridDsp.prepareSampling(traversalRowsForDomain(
                spec.domain,
                spec.maximumFrameCount));
        if (configuration == nullptr) {
            return;
        }

        preparedDomain = spec.domain;
        smoothedMorph.reset(configuration->morph);
        morphInitialized = true;
        trimeshDsp.setGuideCurveProvider(configuration->guideCurveProvider.get());
        trimeshGridDsp.setGuideCurveProvider(configuration->guideCurveProvider.get());

        trimeshDsp.prepare(
                const_cast<Mesh*>(configuration->mesh.get()),
                configuration->morph,
                configuration->primaryViewAxis,
                preparedDomain == PortDomain::TimeSignal,
                preparedDomain);

        trimeshGridDsp.setCyclic(preparedDomain == PortDomain::TimeSignal);
        trimeshGridDsp.prepare(
                *const_cast<Mesh*>(configuration->mesh.get()),
                configuration->morph,
                configuration->primaryViewAxis,
                std::max(kDefaultTraversalColumns, spec.maximumFrameCount / 2),
                traversalRowsForDomain(preparedDomain, spec.maximumFrameCount),
                preparedDomain);
        traversalMorphs.resize(std::max(
                kDefaultTraversalColumns,
                spec.maximumFrameCount / 2));
    }

    void process(AudioProcessContext& context) override {
        AudioOutputPort outputPort;
        if (!context.outputPorts.empty()) {
            outputPort = context.outputPorts.front();
        } else {
            outputPort = {
                    "out",
                    PortDomain::ControlSignal,
                    ChannelLayout::LinkedStereo
            };
        }

        auto output = makeOutputPayload(context, 0);
        output.domain = outputPort.domain;
        output.channelLayout = outputPort.channelLayout;

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const MorphPosition baseMorph = configuration != nullptr
                ? configuration->morph
                : MorphPosition { 0.5f, 0.5f, 0.5f };
        if (!morphInitialized) {
            smoothedMorph.reset(baseMorph);
            morphInitialized = true;
        }

        smoothedMorph.setTargets(morphTargets(context, baseMorph));
        smoothedMorph.advance(context.frameCount, context.timing.sampleRate);

        const MorphPosition& morph = smoothedMorph.current();
        const int primaryAxis = configuration != nullptr
                ? configuration->primaryViewAxis
                : Vertex::Time;
        const auto& voice = processVoice(context);
        const int frequencyMidiNote = voice.controls.noteNumber;
        if (voice.hasLifecycleSeed) {
            trimeshDsp.setVoiceLifecycleSeed(voice.lifecycleSeed);
            trimeshGridDsp.setVoiceLifecycleSeed(voice.lifecycleSeed);
        }
        trimeshDsp.setFrequencyMidiNote(frequencyMidiNote);
        trimeshGridDsp.setFrequencyMidiNote(frequencyMidiNote);

        const SignalPayload* scratch = scratchAttachment(context);
        const auto scratchDomain = scratchDomainFor(outputPort.domain);
        const bool scratchAppliesToBlock = scratch != nullptr
                && !scratch->block.samples.empty()
                && Rasterization::ScratchPositionPolicy::shouldApply(
                        scratchDomain, primaryAxis);
        MorphPosition renderMorph = morph;
        if (scratchAppliesToBlock) {
            renderMorph = Rasterization::ScratchPositionPolicy::resolve(
                    morph,
                    scratchDomain,
                    primaryAxis,
                    scratch->block.samples.front());
        }

        renderBlock(
                context,
                outputPort,
                renderMorph,
                primaryAxis,
                scratchAppliesToBlock,
                output);
        applyGain(output, context.frameCount);

        if (context.captureTraversalGrid) {
            renderTraversal(
                    context,
                    outputPort,
                    morph,
                    primaryAxis,
                    scratch,
                    scratchDomain,
                    output);
            applyTraversalGain(output);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
    void applyGain(SignalPayload& output, size_t frameCount) const {
        if (configuration == nullptr || configuration->gain == 1.f) {
            return;
        }
        payloadBuffer(output, frameCount).mul(configuration->gain);
        if (output.isStereo()) {
            payloadBuffer(output, 1, frameCount).mul(configuration->gain);
        }
    }

    void applyTraversalGain(SignalPayload& output) const {
        if (configuration == nullptr || configuration->gain == 1.f) {
            return;
        }
        Buffer<float>(
                output.traversalGrid.values.data(),
                (int) output.traversalGrid.values.size())
                .mul(configuration->gain);
        if (output.isStereo()) {
            Buffer<float>(
                    output.secondaryTraversalGrid.values.data(),
                    (int) output.secondaryTraversalGrid.values.size())
                    .mul(configuration->gain);
        }
    }

    static float absoluteMorphValue(
            AudioProcessContext& context,
            size_t inputIndex,
            float fallback) {
        const SignalPayload* input = inputAt(context, inputIndex);
        if (input == nullptr || input->block.samples.empty()) {
            return fallback;
        }

        return jlimit(0.f, 1.f, input->block.samples.front());
    }

    static MorphPosition morphTargets(
            AudioProcessContext& context,
            const MorphPosition& fallback) {
        return {
                absoluteMorphValue(context, 2, fallback.time.getCurrentValue()),
                absoluteMorphValue(context, 3, fallback.red.getCurrentValue()),
                absoluteMorphValue(context, 4, fallback.blue.getCurrentValue())
        };
    }

    static bool hasConnectedMorphInput(AudioProcessContext& context) {
        for (size_t inputIndex = 2; inputIndex < 5; ++inputIndex) {
            const SignalPayload* input = inputAt(context, inputIndex);
            if (input != nullptr && !input->block.samples.empty()) {
                return true;
            }
        }

        return false;
    }

    Mesh& currentMesh() {
        return configuration != nullptr
                ? *const_cast<Mesh*>(configuration->mesh.get())
                : fallbackTopology.mesh();
    }

    void renderBlock(
            AudioProcessContext& context,
            const AudioOutputPort& outputPort,
            const MorphPosition& morph,
            int primaryAxis,
            bool renderCurrentMorph,
            SignalPayload& output) {
        if (configuration != nullptr) {
            if (renderCurrentMorph || hasConnectedMorphInput(context)) {
                trimeshDsp.setMorphPosition(morph);
                trimeshDsp.renderCycle(
                        context.frameCount,
                        outputPort.domain,
                        outputPort.channelLayout,
                        output);
                return;
            }

            trimeshDsp.renderPrepared(
                    context.frameCount,
                    outputPort.domain,
                    outputPort.channelLayout,
                    output);
            return;
        }

        Mesh& mesh = fallbackTopology.mesh();
        trimeshDsp.prepare(
                &mesh,
                morph,
                primaryAxis,
                outputPort.domain == PortDomain::TimeSignal,
                outputPort.domain);
        trimeshDsp.renderPrepared(
                context.frameCount,
                outputPort.domain,
                outputPort.channelLayout,
                output);
    }

    void renderTraversal(
            AudioProcessContext& context,
            const AudioOutputPort& outputPort,
            const MorphPosition& morph,
            int primaryAxis,
            const SignalPayload* scratch,
            Rasterization::ScratchSourceDomain scratchDomain,
            SignalPayload& output) {
        const size_t columnCount = std::max(
                kDefaultTraversalColumns,
                context.frameCount / 2);
        const size_t rowCount = traversalRowsForDomain(
                outputPort.domain,
                context.frameCount);

        auto metadata = makeTraversalGridMetadata(
                output.domain,
                columnCount,
                rowCount,
                TraversalGridAxis::Time,
                defaultTraversalRowAxisForDomain(output.domain));
        metadata.frequencyMidiNote = processVoice(context).controls.noteNumber;
        if (metadata.rowAxis == TraversalGridAxis::Frequency) {
            metadata.frequencySampling = TraversalGridFrequencySampling::LinearBins;
        }
        configureTraversalGrid(
                output.traversalGrid,
                columnCount,
                rowCount,
                metadata,
                context.workArena);

        Mesh& mesh = currentMesh();
        trimeshGridDsp.setCyclic(outputPort.domain == PortDomain::TimeSignal);
        if (scratch != nullptr
                && Rasterization::ScratchPositionPolicy::shouldApply(
                        scratchDomain, primaryAxis)
                && traversalMorphs.size() >= columnCount) {
            for (size_t column = 0; column < columnCount; ++column) {
                const MorphPosition columnMorph = TrimeshGridwiseDsp::morphForColumn(
                        morph,
                        primaryAxis,
                        column,
                        columnCount);
                traversalMorphs[column] = Rasterization::ScratchPositionPolicy::resolve(
                        columnMorph,
                        scratchDomain,
                        primaryAxis,
                        scratchCoordinateForColumn(
                                *scratch,
                                column,
                                columnCount,
                                columnMorph.time.getCurrentValue()));
            }
            trimeshGridDsp.renderMorphColumnsInto(
                    mesh,
                    traversalMorphs.data(),
                    primaryAxis,
                    columnCount,
                    Buffer<float>(
                            output.traversalGrid.values.data(),
                            (int) (columnCount * rowCount)),
                    outputPort.domain);
            return;
        }
        trimeshGridDsp.renderColumnsInto(
                mesh,
                morph,
                primaryAxis,
                columnCount,
                Buffer<float>(
                        output.traversalGrid.values.data(),
                        (int) (columnCount * rowCount)),
                outputPort.domain);
    }

    bool morphInitialized {};
    AudioModuleRole processorRole { AudioModuleRole::MeshSource };
    PortDomain preparedDomain { PortDomain::ControlSignal };
    SmoothedMorphPosition smoothedMorph;
    TrimeshBlockwiseDsp trimeshDsp;
    TrimeshGridwiseDsp trimeshGridDsp;
    std::vector<MorphPosition> traversalMorphs;
    PreparedTrimeshTopology fallbackTopology { "CycleV2AudioMesh" };
    std::shared_ptr<const TrimeshConfiguration> configuration;
};

}

std::unique_ptr<NodeAudioProcessor> createTrimeshAudioProcessor() {
    return std::make_unique<TrimeshAudioProcessor>(AudioModuleRole::MeshSource);
}

std::unique_ptr<NodeAudioProcessor> createWaveSourceAudioProcessor() {
    return std::make_unique<TrimeshAudioProcessor>(AudioModuleRole::WaveSource);
}

}
