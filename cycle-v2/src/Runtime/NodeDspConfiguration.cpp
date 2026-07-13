#include "NodeDspConfiguration.h"

#include "../Nodes/Waveshaper/WaveshaperSignalProcessor.h"
#include "../Nodes/Effects/EffectSignalProcessors.h"
#include "../Nodes/Envelope/EnvelopeSignalProcessor.h"

namespace CycleV2 {

String NodeDspConfigurationFactory::keyFor(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const AudioExecutionSpec& spec) const {
    String key((int) role);
    (void) spec;

    for (const auto& parameter : parameters) {
        key << ":" << parameter.id << "=" << parameter.value;
    }

    return key;
}

std::shared_ptr<const INodeDspConfiguration> NodeDspConfigurationFactory::create(
        AudioModuleRole role,
        const std::vector<NodeParameter>& parameters,
        const AudioExecutionSpec&) const {
    switch (role) {
        case AudioModuleRole::Waveshaper:
            return WaveshaperSignalProcessor::buildConfiguration(parameters);

        case AudioModuleRole::ImpulseResponse:
            return IrSignalProcessor::buildConfiguration(parameters);

        case AudioModuleRole::Reverb:
            return ReverbSignalProcessor::buildConfiguration(parameters);

        case AudioModuleRole::Envelope:
            return EnvelopeSignalProcessor::buildConfiguration(parameters);

        default:
            return {};
    }
}

}
