#include "AudioProcessorFactories.h"

#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"
#include "../Nodes/FFT/FftSignalProcessor.h"

namespace CycleV2 {

namespace {

class EnvelopeAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::Envelope; }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        processor.prepareExecution(spec);
    }

    void adoptConfiguration(const PublishedNodeConfiguration& configuration) override {
        processor.adoptConfiguration(configuration);
    }

    bool serviceNonRealtimePreparation() override {
        return processor.serviceNonRealtimePreparation();
    }

    void process(AudioProcessContext& context) override {
        processor.process(context);
    }

private:
    EnvelopeSignalProcessor processor;
};

class FftAudioProcessor final : public NodeAudioProcessor {
public:
    explicit FftAudioProcessor(bool inverse) : inverse(inverse) {}

    AudioModuleRole role() const override {
        return inverse ? AudioModuleRole::Ifft : AudioModuleRole::Fft;
    }

    void prepareExecution(const AudioExecutionSpec& spec) override {
        processor.prepareExecution(spec.maximumFrameCount);
    }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const FftNodeConfiguration>(published.value);
    }

    void process(AudioProcessContext& context) override {
        if (!inverse) {
            processor.processForward(context);
            return;
        }

        if (configuration != nullptr) {
            processor.processInverse(context, configuration->halfCycleCarry);
            return;
        }

        processor.processInverse(context);
    }

private:
    bool inverse {};
    FftSignalProcessor processor;
    std::shared_ptr<const FftNodeConfiguration> configuration;
};

}

std::unique_ptr<NodeAudioProcessor> createEnvelopeAudioProcessor() {
    return std::make_unique<EnvelopeAudioProcessor>();
}

std::unique_ptr<NodeAudioProcessor> createFftAudioProcessor() {
    return std::make_unique<FftAudioProcessor>(false);
}

std::unique_ptr<NodeAudioProcessor> createIfftAudioProcessor() {
    return std::make_unique<FftAudioProcessor>(true);
}

}
