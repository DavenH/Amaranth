#pragma once

#include "Nodes/Control/ModulationSource.h"

namespace CycleV2 {

struct ModulationTripleConfiguration final : public INodeDspConfiguration {
    std::array<ModulationSourceConfiguration, 3> sources;
    std::array<ModulationSourceConfiguration, 3> envelopeSources;

    const ModulationSourceConfiguration& source(size_t axis, bool envelope = false) const {
        return envelope ? envelopeSources[axis] : sources[axis];
    }

    AudioModuleRole role() const override { return AudioModuleRole::ModulationTriple; }
};

std::shared_ptr<const ModulationTripleConfiguration> buildModulationTripleConfiguration(
        const std::vector<NodeParameter>& parameters);
std::unique_ptr<NodeAudioProcessor> createModulationTripleAudioProcessor();

}
