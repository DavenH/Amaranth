#pragma once

#include "AudioProcessTypes.h"
#include "NodeModuleRegistry.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace CycleV2 {

struct AudioExecutionSpec {
    size_t maximumFrameCount {};
    double sampleRate { 44100.0 };
    ChannelLayout channelLayout { ChannelLayout::Mono };
};

class INodeDspConfiguration {
public:
    virtual ~INodeDspConfiguration() = default;

    virtual AudioModuleRole role() const = 0;
};

struct PublishedNodeConfiguration {
    uint64_t revision {};
    String key;
    std::shared_ptr<const INodeDspConfiguration> value;

    bool isValid() const { return revision != 0 && value != nullptr; }
};

class NodeConfigurationPublisher {
public:
    using Builder = std::function<std::shared_ptr<const INodeDspConfiguration>()>;

    const PublishedNodeConfiguration& publish(const String& key, const Builder& builder) {
        if (published.isValid() && published.key == key) {
            return published;
        }

        auto value = builder();
        if (value == nullptr) {
            return published;
        }

        published = { ++lastRevision, key, std::move(value) };
        return published;
    }

    const PublishedNodeConfiguration& current() const { return published; }

private:
    uint64_t lastRevision {};
    PublishedNodeConfiguration published;
};

class NodeDspConfigurationFactory {
public:
    String keyFor(
            AudioModuleRole role,
            const std::vector<NodeParameter>& parameters,
            const AudioExecutionSpec& spec) const;
    std::shared_ptr<const INodeDspConfiguration> create(
            AudioModuleRole role,
            const std::vector<NodeParameter>& parameters,
            const AudioExecutionSpec& spec) const;
};

}
