#pragma once

#include <JuceHeader.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Runtime/NodeAudioProcessor.h"

namespace CycleV2 {

class GraphAudioProcessorCache {
public:
    NodeAudioProcessor* preparedProcessorFor(
            const String& nodeId,
            int voiceIndex,
            AudioModuleRole role,
            const NodeAudioProcessorFactory& factory,
            const PublishedNodeConfiguration& configuration,
            const AudioExecutionSpec& spec);

    void retain(const std::unordered_set<NodeAudioProcessor*>& referencedProcessors);
    void clear();
    size_t preparationCount(const String& nodeId, int voiceIndex) const;
    size_t serviceNonRealtimePreparation();

private:
    struct Key {
        String nodeId;
        int voiceIndex {};

        bool operator==(const Key& other) const {
            return nodeId == other.nodeId && voiceIndex == other.voiceIndex;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const;
    };

    struct PreparationSignature {
        uint64_t revision {};
        String configurationKey;
        size_t maximumFrameCount {};
        size_t traversalColumnCount {};
        double sampleRate {};
        PortDomain domain { PortDomain::ControlSignal };
        ChannelLayout channelLayout { ChannelLayout::Mono };
        double bpm {};
        int beatsPerMeasure {};

        bool operator==(const PreparationSignature& other) const;
    };

    struct Entry {
        AudioModuleRole role { AudioModuleRole::None };
        std::unique_ptr<NodeAudioProcessor> processor;
        PreparationSignature preparation;
        size_t preparationCount {};
        bool prepared {};
    };

    Entry& entryFor(
            const Key& key,
            AudioModuleRole role,
            const NodeAudioProcessorFactory& factory);

    std::unordered_map<Key, Entry, KeyHash> entries;
};

}
