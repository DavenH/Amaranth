#pragma once

#include <Audio/CycleDsp/UnisonCore.h>

#include "../../Runtime/NodeDspConfiguration.h"

namespace CycleV2 {

struct UnisonNodeConfiguration final : public INodeDspConfiguration {
    CycleDsp::UnisonGroupConfiguration group;
    CycleDsp::UnisonVoiceLayout layout;

    AudioModuleRole role() const override { return AudioModuleRole::Unison; }
    bool isEnabled() const override { return group.enabled; }
};

std::shared_ptr<const UnisonNodeConfiguration> buildUnisonNodeConfiguration(
        const std::vector<NodeParameter>& parameters);

}
