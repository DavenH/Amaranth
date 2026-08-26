#include "Nodes/Delay/DelaySignalProcessor.h"

#include "Runtime/AudioProcessContextUtils.h"
#include "Runtime/AudioProcessorFactories.h"
#include "Runtime/NodeAudioProcessorSupport.h"

namespace CycleV2 {

namespace {

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
        const bool enabled = configuration != nullptr && configuration->enabled;
        processUnaryEffect(operation, processor, context, enabled);
    }

private:
    DelaySignalProcessor operation;
    UnarySignalProcessor processor;
    std::shared_ptr<const DelayConfiguration> configuration;
};

}

std::unique_ptr<NodeAudioProcessor> createDelayAudioProcessor() {
    return std::make_unique<DelayAudioProcessor>();
}

}
