#include "UnisonNode.h"

namespace CycleV2 {

std::shared_ptr<const UnisonNodeConfiguration> buildUnisonNodeConfiguration(
        const std::vector<NodeParameter>& parameters) {
    auto configuration = std::make_shared<UnisonNodeConfiguration>();
    configuration->group.enabled = typedParameterBool(parameters, "enabled", true);
    configuration->group.order = typedParameterInt(parameters, "order", 1);
    configuration->group.detuneWidthCents = typedParameterFloat(
            parameters,
            "width",
            CycleDsp::maximumUnisonDetuneCents * 0.5f);
    configuration->group.panSpread = typedParameterFloat(parameters, "panSpread", 1.f);
    configuration->group.phaseSpread = typedParameterFloat(parameters, "phase", 0.5f);
    configuration->group.jitter = typedParameterFloat(parameters, "jitter", 0.5f);
    auto layoutConfiguration = configuration->group;
    layoutConfiguration.enabled = true;
    configuration->layout = CycleDsp::UnisonCore::makeGroupLayout(layoutConfiguration);
    return configuration;
}

}
