#include "Runtime/GraphAudioProcessorCache.h"

namespace CycleV2 {

size_t GraphAudioProcessorCache::KeyHash::operator()(const Key& key) const {
    const size_t nodeHash = static_cast<size_t>(key.nodeId.hashCode64());
    const size_t voiceHash = std::hash<int> {}(key.voiceIndex);
    return nodeHash ^ (voiceHash + 0x9e3779b9 + (nodeHash << 6) + (nodeHash >> 2));
}

bool GraphAudioProcessorCache::PreparationSignature::operator==(
        const PreparationSignature& other) const {
    return revision == other.revision
            && configurationKey == other.configurationKey
            && maximumFrameCount == other.maximumFrameCount
            && traversalColumnCount == other.traversalColumnCount
            && sampleRate == other.sampleRate
            && domain == other.domain
            && channelLayout == other.channelLayout
            && bpm == other.bpm
            && beatsPerMeasure == other.beatsPerMeasure;
}

NodeAudioProcessor* GraphAudioProcessorCache::preparedProcessorFor(
        const String& nodeId,
        int voiceIndex,
        AudioModuleRole role,
        const NodeAudioProcessorFactory& factory,
        const PublishedNodeConfiguration& configuration,
        const AudioExecutionSpec& spec) {
    Entry& entry = entryFor({ nodeId, voiceIndex }, role, factory);
    NodeAudioProcessor* processor = entry.processor.get();
    if (processor == nullptr) {
        return nullptr;
    }

    const PreparationSignature signature {
            configuration.revision,
            configuration.key,
            spec.maximumFrameCount,
            spec.traversalColumnCount,
            spec.sampleRate,
            spec.domain,
            spec.channelLayout,
            spec.bpm,
            spec.beatsPerMeasure
    };
    if (entry.prepared && entry.preparation == signature) {
        return processor;
    }

    processor->adoptConfiguration(configuration);
    processor->prepareExecution(spec);
    entry.preparation = signature;
    entry.prepared = true;
    ++entry.preparationCount;
    return processor;
}

void GraphAudioProcessorCache::retain(
        const std::unordered_set<NodeAudioProcessor*>& referencedProcessors) {
    for (auto entry = entries.begin(); entry != entries.end();) {
        if (referencedProcessors.count(entry->second.processor.get()) == 0) {
            entry = entries.erase(entry);
        } else {
            ++entry;
        }
    }
}

void GraphAudioProcessorCache::clear() {
    entries.clear();
}

size_t GraphAudioProcessorCache::preparationCount(
        const String& nodeId,
        int voiceIndex) const {
    const auto found = entries.find({ nodeId, voiceIndex });
    return found == entries.end() ? 0 : found->second.preparationCount;
}

size_t GraphAudioProcessorCache::serviceNonRealtimePreparation() {
    size_t preparedCount = 0;
    for (auto& [key, entry] : entries) {
        ignoreUnused(key);
        if (entry.processor != nullptr
                && entry.processor->serviceNonRealtimePreparation()) {
            ++preparedCount;
        }
    }
    return preparedCount;
}

GraphAudioProcessorCache::Entry& GraphAudioProcessorCache::entryFor(
        const Key& key,
        AudioModuleRole role,
        const NodeAudioProcessorFactory& factory) {
    const auto found = entries.find(key);
    if (found != entries.end()) {
        Entry& entry = found->second;
        if (entry.role != role) {
            entry.role = role;
            entry.processor = factory.create(role);
            entry.prepared = false;
        }
        return entry;
    }

    auto [inserted, succeeded] = entries.emplace(key, Entry {
            role,
            factory.create(role)
    });
    jassert(succeeded);
    return inserted->second;
}

}
