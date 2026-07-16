#pragma once

#include "AudioProcessTypes.h"
#include "NodeDspConfiguration.h"
#include "NodeModuleRegistry.h"

#include <memory>

namespace CycleV2 {

class NodeAudioProcessor {
public:
    virtual ~NodeAudioProcessor() = default;

    virtual AudioModuleRole role() const = 0;
    virtual void prepareExecution(const AudioExecutionSpec& spec);
    virtual void adoptConfiguration(const PublishedNodeConfiguration& configuration);
    virtual bool serviceNonRealtimePreparation();
    virtual void process(AudioProcessContext& context) = 0;
};

class NodeAudioProcessorFactory {
public:
    std::unique_ptr<NodeAudioProcessor> create(AudioModuleRole role) const;
};

}
