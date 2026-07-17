#include <Array/Buffer.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
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

MorphPosition meshMorphFromParameters(const std::vector<NodeParameter>& parameters) {
    return {
            parameterFloat(parameters, "yellow", 0.5f),
            parameterFloat(parameters, "red", 0.5f),
            parameterFloat(parameters, "blue", 0.5f)
    };
}

class TrimeshAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::MeshSource; }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(published.value);
    }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        if (configuration == nullptr) {
            return;
        }

        preparedDomain = spec.domain;
        smoothedMorph.reset(configuration->morph);
        morphInitialized = true;

        trimeshDsp.prepare(
                configuration->mesh.get(),
                configuration->morph,
                configuration->primaryViewAxis,
                preparedDomain == PortDomain::TimeSignal);

        trimeshGridDsp.setCyclic(preparedDomain == PortDomain::TimeSignal);
        trimeshGridDsp.prepare(
                *configuration->mesh,
                configuration->morph,
                configuration->primaryViewAxis,
                std::max(kDefaultTraversalColumns, spec.maximumFrameCount / 2),
                traversalRowsForDomain(preparedDomain, spec.maximumFrameCount));
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
                : meshMorphFromParameters(processParameters(context));
        if (!morphInitialized) {
            smoothedMorph.reset(baseMorph);
            morphInitialized = true;
        }

        smoothedMorph.setTargets(morphTargets(context, baseMorph));
        smoothedMorph.advance(context.frameCount, context.timing.sampleRate);

        const MorphPosition& morph = smoothedMorph.current();
        const int primaryAxis = configuration != nullptr
                ? configuration->primaryViewAxis
                : primaryAxisFromParameter(typedParameterString(
                        processParameters(context),
                        "primaryAxis",
                        "yellow"));

        renderBlock(context, outputPort, morph, primaryAxis, output);

        if (context.captureTraversalGrid) {
            renderTraversal(context, outputPort, morph, primaryAxis, output);
        }

        publishSingleOutput(context, std::move(output));
    }

private:
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

    Mesh& currentMesh(const std::vector<NodeParameter>& parameters) {
        return configuration != nullptr
                ? *configuration->mesh
                : fallbackTopology.mesh(parameters);
    }

    void renderBlock(
            AudioProcessContext& context,
            const AudioOutputPort& outputPort,
            const MorphPosition& morph,
            int primaryAxis,
            SignalPayload& output) {
        if (configuration != nullptr) {
            if (hasConnectedMorphInput(context)) {
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

        Mesh& mesh = fallbackTopology.mesh(processParameters(context));
        trimeshDsp.prepare(
                &mesh,
                morph,
                primaryAxis,
                outputPort.domain == PortDomain::TimeSignal);
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
            SignalPayload& output) {
        const size_t columnCount = std::max(
                kDefaultTraversalColumns,
                context.frameCount / 2);
        const size_t rowCount = traversalRowsForDomain(
                outputPort.domain,
                context.frameCount);

        configureTraversalGrid(
                output.traversalGrid,
                columnCount,
                rowCount,
                makeTraversalGridMetadata(
                        output.domain,
                        columnCount,
                        rowCount,
                        TraversalGridAxis::Time,
                        defaultTraversalRowAxisForDomain(output.domain)),
                context.workArena);

        Mesh& mesh = currentMesh(processParameters(context));
        trimeshGridDsp.setCyclic(outputPort.domain == PortDomain::TimeSignal);
        trimeshGridDsp.renderColumnsInto(
                mesh,
                morph,
                primaryAxis,
                columnCount,
                Buffer<float>(
                        output.traversalGrid.values.data(),
                        (int) (columnCount * rowCount)));
    }

    bool morphInitialized {};
    PortDomain preparedDomain { PortDomain::ControlSignal };
    SmoothedMorphPosition smoothedMorph;
    TrimeshBlockwiseDsp trimeshDsp;
    TrimeshGridwiseDsp trimeshGridDsp;
    PreparedTrimeshTopology fallbackTopology { "CycleV2AudioMesh" };
    std::shared_ptr<const TrimeshConfiguration> configuration;
};

}

std::unique_ptr<NodeAudioProcessor> createTrimeshAudioProcessor() {
    return std::make_unique<TrimeshAudioProcessor>();
}

}
