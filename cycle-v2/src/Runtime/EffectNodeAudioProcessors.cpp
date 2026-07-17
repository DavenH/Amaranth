#include "AudioProcessContextUtils.h"
#include "AudioProcessorFactories.h"
#include "NodeAudioProcessorSupport.h"

#include "../Nodes/Effects/EffectSignalProcessors.h"
#include "../Nodes/Waveshaper/WaveshaperSignalProcessor.h"

namespace CycleV2 {

namespace {

template<typename Operation, AudioModuleRole Role>
class ConfiguredEffectAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return Role; }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        operation.prepareExecution(spec);
    }

    void adoptConfiguration(const PublishedNodeConfiguration& configuration) override {
        configurationReady = configuration.isValid() && configuration.value->role() == Role;
        configurationEnabled = configurationReady && configuration.value->isEnabled();
        operation.adoptConfiguration(configuration);
    }

    void process(AudioProcessContext& context) override {
        processUnaryEffect(
                operation,
                processor,
                context,
                configurationReady && configurationEnabled);
    }

private:
    bool configurationReady {};
    bool configurationEnabled { true };
    Operation operation;
    UnarySignalProcessor processor;
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

        const bool enabled = configuration != nullptr
                ? configuration->enabled
                : typedParameterBool(processParameters(context), "enabled", true);
        processUnaryEffect(operation, processor, context, enabled);
    }

private:
    DelaySignalProcessor operation;
    UnarySignalProcessor processor;
    std::shared_ptr<const DelayConfiguration> configuration;
};

}

std::unique_ptr<NodeAudioProcessor> createImpulseResponseAudioProcessor() {
    return std::make_unique<ConfiguredEffectAudioProcessor<
            IrSignalProcessor,
            AudioModuleRole::ImpulseResponse>>();
}

std::unique_ptr<NodeAudioProcessor> createWaveshaperAudioProcessor() {
    return std::make_unique<ConfiguredEffectAudioProcessor<
            WaveshaperSignalProcessor,
            AudioModuleRole::Waveshaper>>();
}

std::unique_ptr<NodeAudioProcessor> createReverbAudioProcessor() {
    return std::make_unique<ConfiguredEffectAudioProcessor<
            ReverbSignalProcessor,
            AudioModuleRole::Reverb>>();
}

std::unique_ptr<NodeAudioProcessor> createDelayAudioProcessor() {
    return std::make_unique<DelayAudioProcessor>();
}

}
