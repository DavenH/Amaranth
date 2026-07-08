#pragma once

#include "AudioProcessTypes.h"
#include "NodeModuleRegistry.h"

#include <memory>

namespace CycleV2 {

class NodeAudioProcessor {
public:
    virtual ~NodeAudioProcessor() = default;

    virtual AudioModuleRole role() const = 0;
    virtual void prepare(size_t frameCount);
    virtual void process(AudioProcessContext& context) = 0;
};

class NodeAudioProcessorFactory {
public:
    std::unique_ptr<NodeAudioProcessor> create(AudioModuleRole role) const;
};

}
