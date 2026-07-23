#pragma once

#include "ModulationSource.h"

namespace CycleV2 {

struct ModulationTripleConfiguration final : public INodeDspConfiguration {
    std::array<ModulationSourceConfiguration, 3> sources;

    AudioModuleRole role() const override { return AudioModuleRole::ModulationTriple; }
};

std::shared_ptr<const ModulationTripleConfiguration> buildModulationTripleConfiguration(
        const std::vector<NodeParameter>& parameters);
std::unique_ptr<NodeAudioProcessor> createModulationTripleAudioProcessor();

}
