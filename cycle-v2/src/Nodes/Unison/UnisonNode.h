#pragma once

#include <Audio/CycleDsp/UnisonCore.h>

#include "../../Runtime/NodeDspConfiguration.h"

namespace CycleV2 {

struct UnisonIndividualVoice {
    float detunePosition { 0.5f };
    float pan { 0.5f };
    float phaseCycles {};
};

class UnisonNodeModelState final : public NodeModelState {
public:
    static std::shared_ptr<const UnisonNodeModelState> create(
            std::vector<UnisonIndividualVoice> voices,
            uint64_t revision);

    String schemaId() const override;
    int schemaVersion() const override;
    uint64_t revision() const override;
    var writeJSON() const override;
    bool equals(const NodeModelState& other) const override;

    const std::vector<UnisonIndividualVoice>& voices() const { return individualVoices; }

private:
    UnisonNodeModelState(
            std::vector<UnisonIndividualVoice> voices,
            uint64_t revision);

    std::vector<UnisonIndividualVoice> individualVoices;
    uint64_t modelRevision {};
};

class UnisonNodeModelCodec final : public NodeModelCodec {
public:
    String schemaId() const override;
    int currentVersion() const override;
    NodeModelStatePtr createDefault() const override;
    NodeModelStatePtr readJSON(const var& value, String& error) const override;
};

struct UnisonNodeConfiguration final : public INodeDspConfiguration {
    CycleDsp::UnisonGroupConfiguration group;
    CycleDsp::UnisonVoiceLayout layout;
    bool individualMode {};

    AudioModuleRole role() const override { return AudioModuleRole::Unison; }
    bool isEnabled() const override { return group.enabled; }
};

std::shared_ptr<const UnisonNodeConfiguration> buildUnisonNodeConfiguration(
        const std::vector<NodeParameter>& parameters,
        const NodeModelStatePtr& model = {});

}
