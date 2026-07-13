#include <Array/Buffer.h>

#include "AudioProcessContextUtils.h"
#include "BinarySignalProcessor.h"
#include "NodeAudioProcessor.h"
#include "../Nodes/Effects/EffectSignalProcessors.h"
#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../Nodes/FFT/FftSignalProcessor.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshMeshEditState.h"
#include "../Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../Nodes/Waveshaper/WaveshaperSignalProcessor.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Obj/MorphPosition.h>

namespace CycleV2 {

namespace {

constexpr size_t kDefaultTraversalColumns = 8;

size_t traversalRowsForDomain(PortDomain domain, size_t frameCount) {
    if ((domain == PortDomain::SpectralMagnitudeSignal || domain == PortDomain::SpectralPhaseSignal)
            && frameCount > 1) {
        return frameCount / 2 + 1;
    }

    return frameCount;
}

void publishGridColumns(
        SignalPayload& payload,
        const std::vector<TrimeshGridColumn>& columns,
        const AudioProcessWorkArena* arena) {
    if (columns.empty() || columns.front().signal.block.samples.empty()) {
        payload.traversalGrid = {};
        return;
    }

    const size_t rows = columns.front().signal.block.samples.size();
    configureTraversalGrid(
            payload.traversalGrid,
            columns.size(),
            rows,
            makeTraversalGridMetadata(
                    payload.domain,
                    columns.size(),
                    rows,
                    TraversalGridAxis::Time,
                    defaultTraversalRowAxisForDomain(payload.domain)),
            arena);

    for (size_t column = 0; column < columns.size(); ++column) {
        const auto& samples = columns[column].signal.block.samples;
        const size_t count = std::min(rows, samples.size());
        std::copy(
                samples.begin(),
                samples.begin() + (ptrdiff_t) count,
                payload.traversalGrid.values.begin() + (ptrdiff_t) (column * rows));
    }
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

bool parameterBool(
        const std::vector<NodeParameter>& parameters,
        const String& id,
        bool fallback) {
    const String value = parameterString(parameters, id, fallback ? "1" : "0").toLowerCase();
    return value == "1" || value == "true" || value == "on" || value == "yes";
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

}

class FixedRoleProcessor final : public NodeAudioProcessor {
public:
    explicit FixedRoleProcessor(AudioModuleRole roleToUse) :
            processorRole(roleToUse)
        ,   defaultMesh(TrimeshMeshFactory::createDefaultMesh()) {}

    ~FixedRoleProcessor() override {
        if (defaultMesh != nullptr) {
            defaultMesh->destroy();
        }
    }

    AudioModuleRole role() const override { return processorRole; }

    void process(AudioProcessContext& context) override {
        switch (processorRole) {
            case AudioModuleRole::WaveSource:
                processWaveSource(context);
                break;

            case AudioModuleRole::ImageSource:
                processImageSource(context);
                break;

            case AudioModuleRole::MeshSource:
                processMeshSource(context);
                break;

            case AudioModuleRole::Envelope:
                envelopeDsp.process(context);
                break;

            case AudioModuleRole::Fft:
                fftDsp.processForward(context);
                break;

            case AudioModuleRole::Ifft:
                fftDsp.processInverse(context);
                break;

            case AudioModuleRole::StereoSplit:
                processStereoSplit(context);
                break;

            case AudioModuleRole::StereoJoin:
                processStereoJoin(context);
                break;

            case AudioModuleRole::Add:
                processAdd(context);
                break;

            case AudioModuleRole::Multiply:
                processMultiply(context);
                break;

            case AudioModuleRole::Output:
            case AudioModuleRole::Spy:
            case AudioModuleRole::GenericProcessor:
                processPassthrough(context);
                break;

            case AudioModuleRole::ImpulseResponse:
                processUnaryEffect(irDsp, context);
                break;

            case AudioModuleRole::Waveshaper:
                processUnaryEffect(waveshaperDsp, context);
                break;

            case AudioModuleRole::Reverb:
                processUnaryEffect(reverbDsp, context);
                break;

            case AudioModuleRole::Delay:
                processUnaryEffect(delayDsp, context);
                break;

            case AudioModuleRole::VoiceContext:
            case AudioModuleRole::GuideCurve:
            case AudioModuleRole::None:
            default:
                clearOutput(context);
                break;
        }
    }

private:
    void processWaveSource(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
        const float level = parameterFloat(context.parameters, "level", 1.f);

        payloadBuffer(output, context.frameCount).ramp(0.f, level / denominator);
        publishWrappedRampTraversalGrid(output, kDefaultTraversalColumns, level, context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    void processImageSource(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
        const float level = parameterFloat(context.parameters, "level", 1.f);

        payloadBuffer(output, context.frameCount).ramp(0.f, level / denominator);
        publishImageTraversalGrid(
                output,
                std::max(kDefaultTraversalColumns, context.frameCount),
                level,
                context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    void processMeshSource(AudioProcessContext& context) const {
        AudioOutputPort outputPort;
        if (!context.outputPorts.empty()) {
            outputPort = context.outputPorts.front();
        } else {
            outputPort = { "out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo };
        }

        auto output = makeOutputPayload(outputPort, context.frameCount);

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const auto morph = meshMorphFromParameters(context.parameters);
        const int primaryAxis = primaryAxisFromParameter(
                parameterString(context.parameters, "primaryAxis", "yellow"));
        syncMeshEdits(context.parameters);

        trimeshDsp.setMesh(defaultMesh.get());
        trimeshDsp.setMorphPosition(morph);
        trimeshDsp.setPrimaryViewAxis(primaryAxis);
        trimeshDsp.setCyclic(outputPort.domain == PortDomain::TimeSignal);
        trimeshDsp.renderCycle(context.frameCount, outputPort.domain, outputPort.channelLayout, output);

        publishGridColumns(
                output,
                renderMeshColumns(
                        morph,
                        primaryAxis,
                        std::max(kDefaultTraversalColumns, context.frameCount / 2),
                        traversalRowsForDomain(outputPort.domain, context.frameCount),
                        outputPort.domain,
                        outputPort.channelLayout),
                context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    void processStereoSplit(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto left = makeOutputPayload(context, 0);
        auto right = makeOutputPayload(context, 1);
        copyPayloadBlockExpandingScalars(left, *input, context.frameCount);
        copyPayloadBlockExpandingScalars(right, *input, context.frameCount);
        copyTraversalGrid(left, input->traversalGrid);
        copyTraversalGrid(right, input->traversalGrid);

        publishOutputs(context, std::move(left), std::move(right));
    }

    void processStereoJoin(AudioProcessContext& context) const {
        processAdd(context);
    }

    void processAdd(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);

        binaryDsp.process(output, left, right, BinarySignalOperation::Add, context.frameCount, context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    void processMultiply(AudioProcessContext& context) const {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);

        if (left == nullptr || right == nullptr) {
            clearOutput(context);
            return;
        }

        binaryDsp.process(output, left, right, BinarySignalOperation::Multiply, context.frameCount, context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    void processPassthrough(AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        if (context.outputPorts.empty()) {
            output.domain = input->domain;
            output.channelLayout = input->channelLayout;
        }

        copyPayloadBlockExpandingScalars(output, *input, context.frameCount);
        copyTraversalGrid(output, input->traversalGrid);
        publishSingleOutput(context, std::move(output));
    }

    void processUnaryEffect(IUnarySignalOperation& operation, AudioProcessContext& context) const {
        SignalPayload* input = inputAt(context, 0);

        if (input == nullptr) {
            clearOutput(context);
            return;
        }

        if (!parameterBool(context.parameters, "enabled", true)) {
            processPassthrough(context);
            return;
        }

        auto output = makeOutputPayload(context, 0);
        if (context.outputPorts.empty()) {
            output.domain = input->domain;
            output.channelLayout = input->channelLayout;
        }

        unaryDsp.process(
                operation,
                output,
                *input,
                context.parameters,
                context.timing,
                context.frameCount,
                context.workArena);
        publishSingleOutput(context, std::move(output));
    }

    std::vector<TrimeshGridColumn> renderMeshColumns(
            const MorphPosition& morph,
            int primaryAxis,
            size_t columnCount,
            size_t frameCount,
            PortDomain domain,
            ChannelLayout channelLayout) const {
        TrimeshGridwiseDsp gridwiseDsp;
        gridwiseDsp.setCyclic(domain == PortDomain::TimeSignal);
        return gridwiseDsp.renderColumns(
                *defaultMesh,
                morph,
                primaryAxis,
                columnCount,
                frameCount,
                domain,
                channelLayout);
    }

    void syncMeshEdits(const std::vector<NodeParameter>& parameters) const {
        const TrimeshMeshEditState nextState = TrimeshMeshEditState::fromParameters(parameters);

        if (nextState == meshEditState) {
            return;
        }

        defaultMesh->destroy();
        defaultMesh = TrimeshMeshFactory::createDefaultMesh();
        nextState.applyTo(*defaultMesh);
        meshEditState = nextState;
    }

    static void publishWrappedRampTraversalGrid(
            SignalPayload& payload,
            size_t columns,
            float level,
            const AudioProcessWorkArena* arena) {
        if (payload.block.samples.empty()) {
            payload.traversalGrid = {};
            return;
        }

        const size_t rows = payload.block.samples.size();
        configureTraversalGrid(
                payload.traversalGrid,
                columns,
                rows,
                makeTraversalGridMetadata(
                        payload.domain,
                        columns,
                        rows,
                        TraversalGridAxis::Phase,
                        TraversalGridAxis::Time),
                arena);
        const float rowDenominator = rows > 1 ? (float) (rows - 1) : 1.f;
        const float columnDenominator = columns > 0 ? (float) columns : 1.f;

        for (size_t column = 0; column < columns; ++column) {
            const float phaseOffset = (float) column / columnDenominator;
            for (size_t row = 0; row < rows; ++row) {
                float value = (float) row / rowDenominator + phaseOffset;
                if (value >= 1.f) {
                    value -= 1.f;
                }
                payload.traversalGrid.values[column * rows + row] = value * level;
            }
        }
    }

    static void publishImageTraversalGrid(
            SignalPayload& payload,
            size_t columns,
            float level,
            const AudioProcessWorkArena* arena) {
        if (payload.block.samples.empty()) {
            payload.traversalGrid = {};
            return;
        }

        const size_t rows = payload.block.samples.size();
        configureTraversalGrid(
                payload.traversalGrid,
                columns,
                rows,
                makeTraversalGridMetadata(
                        payload.domain,
                        columns,
                        rows,
                        TraversalGridAxis::ImageX,
                        TraversalGridAxis::ImageY),
                arena);
        const float rowDenominator = rows > 1 ? (float) (rows - 1) : 1.f;
        const float columnDenominator = columns > 1 ? (float) (columns - 1) : 1.f;

        for (size_t column = 0; column < columns; ++column) {
            const float x = (float) column / columnDenominator;
            for (size_t row = 0; row < rows; ++row) {
                const float y = (float) row / rowDenominator;
                payload.traversalGrid.values[column * rows + row] = level * (0.65f * x + 0.35f * y);
            }
        }
    }

    AudioModuleRole processorRole {};
    mutable BinarySignalProcessor binaryDsp;
    mutable DelaySignalProcessor delayDsp;
    EnvelopeSignalProcessor envelopeDsp;
    mutable FftSignalProcessor fftDsp;
    mutable IrSignalProcessor irDsp;
    mutable ReverbSignalProcessor reverbDsp;
    mutable TrimeshBlockwiseDsp trimeshDsp;
    mutable UnarySignalProcessor unaryDsp;
    mutable WaveshaperSignalProcessor waveshaperDsp;
    mutable std::unique_ptr<Mesh> defaultMesh;
    mutable TrimeshMeshEditState meshEditState;
};

void NodeAudioProcessor::prepare(size_t) {}

std::unique_ptr<NodeAudioProcessor> NodeAudioProcessorFactory::create(AudioModuleRole role) const {
    if (role == AudioModuleRole::None) {
        return {};
    }

    return std::make_unique<FixedRoleProcessor>(role);
}

}
