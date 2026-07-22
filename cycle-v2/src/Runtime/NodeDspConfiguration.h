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
    PortDomain domain { PortDomain::ControlSignal };
    double bpm { 120.0 };
    int beatsPerMeasure { 4 };
};

class INodeDspConfiguration {
public:
    virtual ~INodeDspConfiguration() = default;

    virtual AudioModuleRole role() const = 0;
    virtual bool isEnabled() const { return true; }
};

struct SourceNodeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole processorRole { AudioModuleRole::None };
    float level { 1.f };

    AudioModuleRole role() const override { return processorRole; }
};

struct FftNodeConfiguration final : public INodeDspConfiguration {
    AudioModuleRole processorRole { AudioModuleRole::None };
    bool halfCycleCarry {};

    AudioModuleRole role() const override { return processorRole; }
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
            const NodeModelStatePtr& model,
            const AudioExecutionSpec& spec) const;
    std::shared_ptr<const INodeDspConfiguration> create(
            AudioModuleRole role,
            const std::vector<NodeParameter>& parameters,
            const NodeModelStatePtr& model,
            const AudioExecutionSpec& spec) const;
    std::shared_ptr<const INodeDspConfiguration> create(
            AudioModuleRole role,
            const std::vector<NodeParameter>& parameters,
            const AudioExecutionSpec& spec) const {
        return create(role, parameters, {}, spec);
    }
};

}
