#include "Nodes/Control/ModulationTriple.h"

#include "Runtime/AudioProcessContextUtils.h"

namespace CycleV2 {

namespace {

class ModulationTripleAudioProcessor final : public NodeAudioProcessor {
public:
    AudioModuleRole role() const override { return AudioModuleRole::ModulationTriple; }

    void adoptConfiguration(const PublishedNodeConfiguration& published) override {
        configuration = std::dynamic_pointer_cast<const ModulationTripleConfiguration>(
                published.value);
    }

    void process(AudioProcessContext& context) override {
        context.outputs.clear();
        if (context.workArena != nullptr) {
            context.outputs.reserve(context.workArena->outputCapacity);
        }

        const AudioVoiceContext& voice = processVoice(context);
        for (size_t index = 0; index < 3; ++index) {
            auto output = makeOutputPayload(context, index);
            auto values = payloadBuffer(output, context.frameCount);
            if (configuration == nullptr) {
                values.zero();
            } else {
                ModulationSource::renderAudioBlock(
                        configuration->sources[index],
                        voice,
                        values);
            }
            context.outputs.push_back(std::move(output));
        }
    }

private:
    std::shared_ptr<const ModulationTripleConfiguration> configuration;
};

}

std::shared_ptr<const ModulationTripleConfiguration> buildModulationTripleConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto configuration = std::make_shared<ModulationTripleConfiguration>();
    configuration->sources = {
            ModulationSource::buildConfiguration(parameters, "yellow", "voiceTime"),
            ModulationSource::buildConfiguration(parameters, "red", "keyScale"),
            ModulationSource::buildConfiguration(parameters, "blue", "modWheel")
    };
    return configuration;
}

std::unique_ptr<NodeAudioProcessor> createModulationTripleAudioProcessor() {
    return std::make_unique<ModulationTripleAudioProcessor>();
}

}
