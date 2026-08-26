#include "Runtime/AudioProcessContextUtils.h"
#include "Runtime/AudioProcessorFactories.h"
#include "Runtime/NodeAudioProcessorSupport.h"

#include "Nodes/Effects/EffectSignalProcessors.h"
#include "Nodes/Waveshaper/WaveshaperSignalProcessor.h"

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

std::unique_ptr<NodeAudioProcessor> createEqualizerAudioProcessor() {
    return std::make_unique<ConfiguredEffectAudioProcessor<
            EqualizerSignalProcessor,
            AudioModuleRole::Equalizer>>();
}

}
