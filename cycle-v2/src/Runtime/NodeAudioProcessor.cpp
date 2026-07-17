#include <Array/Buffer.h>

#include "AudioProcessContextUtils.h"
#include "BinarySignalProcessor.h"
#include "NodeAudioProcessor.h"
#include "SmoothedMorphPosition.h"
#include "../Nodes/Effects/EffectSignalProcessors.h"
#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../Nodes/FFT/FftSignalProcessor.h"
#include "../Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../Nodes/Trimesh/PreparedTrimeshTopology.h"
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

void processPassthrough(AudioProcessContext& context) {
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

void processUnaryEffect(
        IUnarySignalOperation& operation,
        UnarySignalProcessor& processor,
        AudioProcessContext& context,
        bool enabled) {
    SignalPayload* input = inputAt(context, 0);
    if (input == nullptr) {
        clearOutput(context);
        return;
    }
    if (!enabled) {
        processPassthrough(context);
        return;
    }

    auto output = makeOutputPayload(context, 0);
    if (context.outputPorts.empty()) {
        output.domain = input->domain;
        output.channelLayout = input->channelLayout;
    }
    processor.process(operation, output, *input, context.frameCount, context.workArena);
    publishSingleOutput(context, std::move(output));
}

void publishSourceTraversalGrid(
        SignalPayload& payload,
        size_t columns,
        float level,
        bool image,
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
                    image ? TraversalGridAxis::ImageX : TraversalGridAxis::Phase,
                    image ? TraversalGridAxis::ImageY : TraversalGridAxis::Time),
            arena);
    const float rowDenominator = rows > 1 ? (float) (rows - 1) : 1.f;
    const float columnDenominator = columns > (image ? 1u : 0u) ? (float) (columns - (image ? 1u : 0u)) : 1.f;

    for (size_t column = 0; column < columns; ++column) {
        const float columnPosition = (float) column / columnDenominator;
        for (size_t row = 0; row < rows; ++row) {
            const float rowPosition = (float) row / rowDenominator;
            if (image) {
                payload.traversalGrid.values[column * rows + row]
                        = level * (0.65f * columnPosition + 0.35f * rowPosition);
            } else {
                float value = rowPosition + columnPosition;
                if (value >= 1.f) {
                    value -= 1.f;
                }
                payload.traversalGrid.values[column * rows + row] = value * level;
            }
        }
    }
}

}

class SourceAudioProcessor final : public NodeAudioProcessor {
public:
    explicit SourceAudioProcessor(bool image) : image(image) {}
    AudioModuleRole role() const override {
        return image ? AudioModuleRole::ImageSource : AudioModuleRole::WaveSource;
    }
    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const SourceNodeConfiguration>(published.value);
    }
    void process(AudioProcessContext& context) override {
        auto output = makeOutputPayload(context, 0);
        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const float denominator = context.frameCount > 1 ? (float) (context.frameCount - 1) : 1.f;
        const float level = configuration != nullptr
                ? configuration->level
                : parameterFloat(processParameters(context), "level", 1.f);
        payloadBuffer(output, context.frameCount).ramp(0.f, level / denominator);
        if (context.captureTraversalGrid) {
            publishSourceTraversalGrid(
                    output,
                    image ? std::max(kDefaultTraversalColumns, context.frameCount) : kDefaultTraversalColumns,
                    level,
                    image,
                    context.workArena);
        }
        publishSingleOutput(context, std::move(output));
    }

private:
    bool image {};
    std::shared_ptr<const SourceNodeConfiguration> configuration;
};

class BinaryAudioProcessor final : public NodeAudioProcessor {
public:
    BinaryAudioProcessor(AudioModuleRole role, BinarySignalOperation operation) :
            processorRole(role), operation(operation) {}
    AudioModuleRole role() const override { return processorRole; }
    void process(AudioProcessContext& context) override {
        auto output = makeOutputPayload(context, 0);
        SignalPayload* left = inputAt(context, 0);
        SignalPayload* right = inputAt(context, 1);
        if (operation == BinarySignalOperation::Multiply && (left == nullptr || right == nullptr)) {
            clearOutput(context);
            return;
        }
        processor.process(output, left, right, operation, context.frameCount, context.workArena);
        publishSingleOutput(context, std::move(output));
    }

private:
    AudioModuleRole processorRole {};
    BinarySignalOperation operation { BinarySignalOperation::Add };
    BinarySignalProcessor processor;
};

class PassthroughAudioProcessor final : public NodeAudioProcessor {
public:
    explicit PassthroughAudioProcessor(AudioModuleRole role) : processorRole(role) {}
    AudioModuleRole role() const override { return processorRole; }
    void process(AudioProcessContext& context) override { processPassthrough(context); }

private:
    AudioModuleRole processorRole {};
};

class SilentAudioProcessor final : public NodeAudioProcessor {
public:
    explicit SilentAudioProcessor(AudioModuleRole role) : processorRole(role) {}
    AudioModuleRole role() const override { return processorRole; }
    void process(AudioProcessContext& context) override { clearOutput(context); }

private:
    AudioModuleRole processorRole {};
};

class StereoSplitAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::StereoSplit; }
    void process(AudioProcessContext& context) override {
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
};

class EnvelopeAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }
    void prepareExecution(const AudioExecutionSpec& spec) override { processor.prepareExecution(spec); }
    void adoptConfiguration(const PublishedNodeConfiguration& configuration) override {
        processor.adoptConfiguration(configuration);
    }
    bool serviceNonRealtimePreparation() override {
        return processor.serviceNonRealtimePreparation();
    }
    void process(AudioProcessContext& context) override { processor.process(context); }

private:
    EnvelopeSignalProcessor processor;
};

class FftAudioProcessor final : public NodeAudioProcessor {
public:
    explicit FftAudioProcessor(bool inverse) : inverse(inverse) {}
    AudioModuleRole role() const override { return inverse ? AudioModuleRole::Ifft : AudioModuleRole::Fft; }
    void prepareExecution(const AudioExecutionSpec& spec) override {
        processor.prepareExecution(spec.maximumFrameCount);
    }
    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const FftNodeConfiguration>(published.value);
    }
    void process(AudioProcessContext& context) override {
        if (inverse) {
            if (configuration != nullptr) {
                processor.processInverse(context, configuration->halfCycleCarry);
            } else {
                processor.processInverse(context);
            }
        } else {
            processor.processForward(context);
        }
    }

private:
    bool inverse {};
    FftSignalProcessor processor;
    std::shared_ptr<const FftNodeConfiguration> configuration;
};

template<typename Operation, AudioModuleRole Role>
class ConfiguredEffectAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return Role; }
    void prepareExecution(const AudioExecutionSpec& spec) override { operation.prepareExecution(spec); }
    void adoptConfiguration(const PublishedNodeConfiguration& configuration) override {
        configurationReady = configuration.isValid() && configuration.value->role() == Role;
        configurationEnabled = configurationReady && configuration.value->isEnabled();
        operation.adoptConfiguration(configuration);
    }
    void process(AudioProcessContext& context) override {
        processUnaryEffect(operation, processor, context, configurationReady && configurationEnabled);
    }

private:
    Operation operation;
    UnarySignalProcessor processor;
    bool configurationReady {};
    bool configurationEnabled { true };
};

class DelayAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::Delay; }
    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const DelayConfiguration>(published.value);
    }
    void prepareExecution(const AudioExecutionSpec& spec) override {
        if (configuration == nullptr) {
            return;
        }
        AudioProcessTiming timing;
        timing.sampleRate = spec.sampleRate;
        timing.bpm = spec.bpm;
        timing.beatsPerMeasure = spec.beatsPerMeasure;
        operation.configure(*configuration, timing);
    }
    void process(AudioProcessContext& context) override {
        if (configuration == nullptr) {
            operation.configure(processParameters(context), context.timing);
        }
        processUnaryEffect(
                operation,
                processor,
                context,
                configuration != nullptr
                        ? configuration->enabled
                        : typedParameterBool(processParameters(context), "enabled", true));
    }

private:
    DelaySignalProcessor operation;
    UnarySignalProcessor processor;
    std::shared_ptr<const DelayConfiguration> configuration;
};

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
        processMeshSource(context);
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

    void processMeshSource(AudioProcessContext& context) {
        AudioOutputPort outputPort;
        if (!context.outputPorts.empty()) {
            outputPort = context.outputPorts.front();
        } else {
            outputPort = { "out", PortDomain::ControlSignal, ChannelLayout::LinkedStereo };
        }

        auto output = makeOutputPayload(context, 0);
        output.domain = outputPort.domain;
        output.channelLayout = outputPort.channelLayout;

        if (context.frameCount == 0) {
            publishSingleOutput(context, std::move(output));
            return;
        }

        const auto baseMorph = configuration != nullptr
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
                        processParameters(context), "primaryAxis", "yellow"));
        if (configuration != nullptr) {
            if (hasConnectedMorphInput(context)) {
                trimeshDsp.setMorphPosition(morph);
                trimeshDsp.renderCycle(
                        context.frameCount,
                        outputPort.domain,
                        outputPort.channelLayout,
                        output);
            } else {
                trimeshDsp.renderPrepared(
                        context.frameCount,
                        outputPort.domain,
                        outputPort.channelLayout,
                        output);
            }
        } else {
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

        if (context.captureTraversalGrid) {
            Mesh& gridMesh = configuration != nullptr
                    ? *configuration->mesh
                    : fallbackTopology.mesh(processParameters(context));
            const size_t columnCount = std::max(
                    kDefaultTraversalColumns, context.frameCount / 2);
            const size_t rowCount = traversalRowsForDomain(
                    outputPort.domain, context.frameCount);
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
            trimeshGridDsp.setCyclic(outputPort.domain == PortDomain::TimeSignal);
            trimeshGridDsp.renderColumnsInto(
                    gridMesh,
                    morph,
                    primaryAxis,
                    columnCount,
                    Buffer<float>(
                            output.traversalGrid.values.data(),
                            (int) (columnCount * rowCount)));
        }
        publishSingleOutput(context, std::move(output));
    }

    bool morphInitialized {};
    PortDomain preparedDomain { PortDomain::ControlSignal };
    SmoothedMorphPosition smoothedMorph;
    TrimeshBlockwiseDsp trimeshDsp;
    TrimeshGridwiseDsp trimeshGridDsp;
    PreparedTrimeshTopology fallbackTopology { "CycleV2AudioMesh" };
    std::shared_ptr<const TrimeshConfiguration> configuration;
};

void NodeAudioProcessor::prepareExecution(const AudioExecutionSpec&) {}

void NodeAudioProcessor::adoptConfiguration(const PublishedNodeConfiguration&) {}

bool NodeAudioProcessor::serviceNonRealtimePreparation() { return false; }

std::unique_ptr<NodeAudioProcessor> NodeAudioProcessorFactory::create(AudioModuleRole role) const {
    using Factory = std::unique_ptr<NodeAudioProcessor> (*)();
    struct Registration {
        AudioModuleRole role;
        Factory factory;
    };
    static const Registration registrations[] {
            { AudioModuleRole::WaveSource, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<SourceAudioProcessor>(false); } },
            { AudioModuleRole::ImageSource, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<SourceAudioProcessor>(true); } },
            { AudioModuleRole::MeshSource, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<TrimeshAudioProcessor>(); } },
            { AudioModuleRole::Add, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<BinaryAudioProcessor>(AudioModuleRole::Add, BinarySignalOperation::Add); } },
            { AudioModuleRole::Multiply, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<BinaryAudioProcessor>(AudioModuleRole::Multiply, BinarySignalOperation::Multiply); } },
            { AudioModuleRole::StereoJoin, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<BinaryAudioProcessor>(AudioModuleRole::StereoJoin, BinarySignalOperation::Add); } },
            { AudioModuleRole::StereoSplit, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<StereoSplitAudioProcessor>(); } },
            { AudioModuleRole::Output, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<PassthroughAudioProcessor>(AudioModuleRole::Output); } },
            { AudioModuleRole::Spy, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<PassthroughAudioProcessor>(AudioModuleRole::Spy); } },
            { AudioModuleRole::GenericProcessor, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<PassthroughAudioProcessor>(AudioModuleRole::GenericProcessor); } },
            { AudioModuleRole::VoiceContext, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<SilentAudioProcessor>(AudioModuleRole::VoiceContext); } },
            { AudioModuleRole::GuideCurve, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<SilentAudioProcessor>(AudioModuleRole::GuideCurve); } },
            { AudioModuleRole::Envelope, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<EnvelopeAudioProcessor>(); } },
            { AudioModuleRole::Fft, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<FftAudioProcessor>(false); } },
            { AudioModuleRole::Ifft, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<FftAudioProcessor>(true); } },
            { AudioModuleRole::ImpulseResponse, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<ConfiguredEffectAudioProcessor<IrSignalProcessor, AudioModuleRole::ImpulseResponse>>(); } },
            { AudioModuleRole::Waveshaper, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<ConfiguredEffectAudioProcessor<WaveshaperSignalProcessor, AudioModuleRole::Waveshaper>>(); } },
            { AudioModuleRole::Reverb, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<ConfiguredEffectAudioProcessor<ReverbSignalProcessor, AudioModuleRole::Reverb>>(); } },
            { AudioModuleRole::Delay, []() -> std::unique_ptr<NodeAudioProcessor> { return std::make_unique<DelayAudioProcessor>(); } }
    };

    for (const auto& registration : registrations) {
        if (registration.role == role) {
            return registration.factory();
        }
    }

    return {};
}

}
