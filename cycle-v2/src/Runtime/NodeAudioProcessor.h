#pragma once

#include "NodeModuleRegistry.h"

#include <memory>
#include <vector>

namespace CycleV2 {

struct AudioProcessBlock {
    std::vector<float> samples;
};

struct AudioProcessContext {
    size_t frameCount {};
    std::vector<AudioProcessBlock> inputs;
    AudioProcessBlock output;
};

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
