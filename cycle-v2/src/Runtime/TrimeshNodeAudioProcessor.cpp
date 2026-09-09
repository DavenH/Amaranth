#include <Array/Buffer.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

#include "Runtime/AudioProcessContextUtils.h"
#include "Runtime/AudioProcessorFactories.h"
#include "Runtime/TrimeshMorphResolver.h"

#include "Nodes/Trimesh/Model/PreparedTrimeshTopology.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Dsp/TrimeshGridwiseDsp.h"

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
        morphResolver.reset(configuration->morph);
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

        const SignalPayload* scratch = configuration == nullptr
                        || configuration->scratchSourceEnabled
                ? scratchAttachment(context)
                : nullptr;
        const auto scratchDomain = TrimeshMorphResolver::domainFor(outputPort.domain);
        const bool scratchAppliesToBlock = scratch != nullptr
                && !scratch->block.samples.empty()
                && Rasterization::ScratchPositionPolicy::shouldApply(
                        scratchDomain, primaryAxis);
        const TrimeshMorphInputs morphInputs {
                {
                        inputAt(context, 2),
                        inputAt(context, 3),
                        inputAt(context, 4)
                },
                {},
                {},
                scratch
        };
        const MorphPosition renderMorph = morphResolver.resolve(
                morphInputs,
                baseMorph,
                outputPort.domain,
                primaryAxis,
                0,
                context.frameCount,
                context.timing.sampleRate,
                configuration == nullptr || configuration->scratchSourceEnabled);
        const MorphPosition& morph = morphResolver.current();

        renderBlock(
                context,
                outputPort,
                renderMorph,
                primaryAxis,
                scratchAppliesToBlock,
                output);
        applyGain(output, context.frameCount);
        applySpectralRange(outputPort.domain, output, context.frameCount);
        applyEnabledIdentity(outputPort.domain, output, context.frameCount);

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
            applySpectralRange(outputPort.domain, output.traversalGrid);
            applySpectralRange(outputPort.domain, output.secondaryTraversalGrid);
            applyEnabledIdentity(outputPort.domain, output.traversalGrid);
            applyEnabledIdentity(outputPort.domain, output.secondaryTraversalGrid);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
    void shapeSpectralValues(PortDomain domain, Buffer<float> values) const {
        if (configuration == nullptr || !configuration->appliesSpectralRange) {
            return;
        }
        if (domain == PortDomain::SpectralMagnitudeSignal) {
            CycleDsp::SpectralLayerCore::shapeMagnitude(
                    values,
                    configuration->range,
                    !configuration->multiplicative,
                    values.size());
        } else if (domain == PortDomain::SpectralPhaseSignal) {
            values.mul(CycleDsp::SpectralLayerCore::phaseOffsetScale(configuration->range)
                    * MathConstants<float>::twoPi);
        }
    }

    void applySpectralRange(
            PortDomain domain,
            SignalPayload& output,
            size_t frameCount) const {
        shapeSpectralValues(domain, payloadBuffer(output, frameCount));
        if (output.isStereo()) {
            shapeSpectralValues(domain, payloadBuffer(output, 1, frameCount));
        }
    }

    void applySpectralRange(PortDomain domain, SignalTraversalGrid& grid) const {
        if (!grid.isValid()) {
            return;
        }
        if (domain == PortDomain::SpectralMagnitudeSignal) {
            for (size_t column = 0; column < grid.columns; ++column) {
                shapeSpectralValues(
                        domain,
                        Buffer<float>(
                                grid.values.data() + column * grid.rows,
                                (int) grid.rows));
            }
            return;
        }
        shapeSpectralValues(
                domain,
                Buffer<float>(grid.values.data(), (int) grid.values.size()));
    }

    float disabledIdentity(PortDomain domain) const {
        return domain == PortDomain::SpectralMagnitudeSignal
                        && configuration != nullptr
                        && configuration->multiplicative
                ? 1.f
                : 0.f;
    }

    void applyEnabledIdentity(
            PortDomain domain,
            SignalPayload& output,
            size_t frameCount) const {
        if (configuration == nullptr || configuration->enabled) {
            return;
        }
        const float identity = disabledIdentity(domain);
        payloadBuffer(output, frameCount).set(identity);
        if (output.isStereo()) {
            payloadBuffer(output, 1, frameCount).set(identity);
        }
    }

    void applyEnabledIdentity(PortDomain domain, SignalTraversalGrid& grid) const {
        if (configuration == nullptr || configuration->enabled || !grid.isValid()) {
            return;
        }
        Buffer<float>(grid.values.data(), (int) grid.values.size()).set(
                disabledIdentity(domain));
    }

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

    AudioModuleRole processorRole { AudioModuleRole::MeshSource };
    PortDomain preparedDomain { PortDomain::ControlSignal };
    TrimeshMorphResolver morphResolver;
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
