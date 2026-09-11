#include "Runtime/PreparedCycleEnvelopeBank.h"

#include "Nodes/Envelope/CycleEnvelopePlaybackSource.h"
#include "Runtime/NodeAudioProcessor.h"

#include <algorithm>

namespace CycleV2 {

bool PreparedCycleEnvelopeBank::prepare(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const std::vector<NodeAudioProcessor*>& processors,
        int laneCount,
        const String& pitchEnvelopeNodeId) {
    entries.clear();
    pitchEntryIndex = -1;
    if (laneCount < 1) {
        return false;
    }

    for (const int stepIndex : region.stepIndices) {
        if (stepIndex < 0 || stepIndex >= (int) plan.steps.size()) {
            return false;
        }
        for (const auto& attachment : plan.steps[(size_t) stepIndex].attachments) {
            if (attachment.destPortId != "scratch"
                    || contains(attachment.sourceBufferIndex)
                    || attachment.sourceBufferIndex < 0
                    || attachment.sourceBufferIndex >= (int) plan.buffers.size()) {
                continue;
            }
            const int sourceStep = plan.buffers[(size_t) attachment.sourceBufferIndex]
                    .firstProducerStep;
            if (sourceStep < 0 || sourceStep >= (int) processors.size()
                    || processors[(size_t) sourceStep] == nullptr) {
                continue;
            }
            const auto* source = processors[(size_t) sourceStep]
                    ->cycleEnvelopePlaybackSource();
            if (source == nullptr) {
                continue;
            }

            auto entry = std::make_unique<Entry>();
            entry->bufferIndex = attachment.sourceBufferIndex;
            entry->laneCount = laneCount;
            entry->source = source;
            entry->values.resize((size_t) laneCount);
            entry->active.resize((size_t) laneCount);
            entry->playback.ensureVoiceCount(laneCount);
            entry->playback.setOneSamplePerCycle(true);
            entries.push_back(std::move(entry));
        }
    }
    if (pitchEnvelopeNodeId.isNotEmpty()) {
        const auto found = plan.dependencyIndex.stepIndexById.find(
                pitchEnvelopeNodeId);
        if (found == plan.dependencyIndex.stepIndexById.end()
                || found->second < 0
                || found->second >= (int) processors.size()
                || processors[(size_t) found->second] == nullptr) {
            return false;
        }
        const auto* source = processors[(size_t) found->second]
                ->cycleEnvelopePlaybackSource();
        if (source == nullptr) {
            return false;
        }
        auto entry = std::make_unique<Entry>();
        entry->laneCount = laneCount;
        entry->pitch = true;
        entry->source = source;
        entry->values.resize((size_t) laneCount, 0.5f);
        entry->active.resize((size_t) laneCount);
        entry->playback.ensureVoiceCount(laneCount);
        entry->playback.setOneSamplePerCycle(true);
        entries.push_back(std::move(entry));
        pitchEntryIndex = (int) entries.size() - 1;
    }
    reset();
    return true;
}

void PreparedCycleEnvelopeBank::reset() {
    for (auto& ownedEntry : entries) {
        Entry& entry = *ownedEntry;
        entry.playback.noteOn();
        entry.adoptedConfiguration = nullptr;
        std::fill(
                entry.values.begin(),
                entry.values.end(),
                entry.pitch ? 0.5f : 0.f);
        std::fill(entry.active.begin(), entry.active.end(), false);
    }
}

void PreparedCycleEnvelopeBank::applyLifecycleEvent(
        const NoteLifecycleEvent& event) {
    for (auto& ownedEntry : entries) {
        Entry& entry = *ownedEntry;
        const EnvelopeConfiguration* configuration = adopt(entry);
        if (configuration == nullptr) {
            continue;
        }
        if (event.type == NoteLifecycleType::NoteOn) {
            entry.playback.noteOn();
            std::fill(entry.active.begin(), entry.active.end(), true);
        } else if (event.type == NoteLifecycleType::NoteOff) {
            const bool releases = entry.playback.noteOff(
                    entry.source->cycleEnvelopePlaybackView());
            if (!releases) {
                std::fill(entry.active.begin(), entry.active.end(), false);
            }
        } else {
            entry.playback.noteOn();
            std::fill(entry.active.begin(), entry.active.end(), false);
        }
    }
}

void PreparedCycleEnvelopeBank::advanceLane(
        int laneIndex,
        int sampleCount,
        double normalizedTimeIncrement) {
    for (auto& entry : entries) {
        advance(*entry, laneIndex, sampleCount, normalizedTimeIncrement);
    }
}

void PreparedCycleEnvelopeBank::advanceAll(
        int sampleCount,
        double normalizedTimeIncrement) {
    for (auto& ownedEntry : entries) {
        Entry& entry = *ownedEntry;
        for (int laneIndex = 0; laneIndex < entry.laneCount; ++laneIndex) {
            advance(entry, laneIndex, sampleCount, normalizedTimeIncrement);
        }
    }
}

bool PreparedCycleEnvelopeBank::contains(int bufferIndex) const {
    return entryFor(bufferIndex) != nullptr;
}

float PreparedCycleEnvelopeBank::value(int bufferIndex, int laneIndex) const {
    const Entry* entry = entryFor(bufferIndex);
    if (entry == nullptr || entry->values.empty()) {
        return 0.f;
    }
    const int index = laneIndex >= 0
            ? std::min(laneIndex, entry->laneCount - 1)
            : entry->laneCount - 1;
    return entry->values[(size_t) index];
}

bool PreparedCycleEnvelopeBank::hasPitchEnvelope() const {
    return pitchEntryIndex >= 0;
}

float PreparedCycleEnvelopeBank::pitchValue(int laneIndex) const {
    if (pitchEntryIndex < 0 || pitchEntryIndex >= (int) entries.size()) {
        return 0.5f;
    }
    const Entry& entry = *entries[(size_t) pitchEntryIndex];
    if (entry.values.empty()) {
        return 0.5f;
    }
    const int index = std::max(0, std::min(laneIndex, entry.laneCount - 1));
    return entry.values[(size_t) index];
}

const EnvelopeConfiguration* PreparedCycleEnvelopeBank::adopt(Entry& entry) {
    const EnvelopeConfiguration* configuration = entry.source == nullptr
            ? nullptr
            : entry.source->cycleEnvelopeConfiguration();
    if (configuration != nullptr
            && configuration != entry.adoptedConfiguration) {
        entry.playback.validate(entry.source->cycleEnvelopePlaybackView());
        entry.props.logarithmic = configuration->logarithmic;
        entry.adoptedConfiguration = configuration;
    }
    return configuration;
}

void PreparedCycleEnvelopeBank::advance(
        Entry& entry,
        int laneIndex,
        int sampleCount,
        double normalizedTimeIncrement) {
    if (laneIndex < 0 || laneIndex >= entry.laneCount
            || sampleCount <= 0 || normalizedTimeIncrement <= 0.) {
        return;
    }
    const EnvelopeConfiguration* configuration = adopt(entry);
    if (configuration == nullptr) {
        return;
    }
    if (!configuration->enabled) {
        entry.values[(size_t) laneIndex] = configuration->neutralValue;
        return;
    }
    if (!entry.active[(size_t) laneIndex]) {
        return;
    }

    const int voiceIndex = Rasterization::EnvelopePlaybackEngine::firstAudioVoiceIndex
            + laneIndex;
    entry.active[(size_t) laneIndex] = entry.playback.renderToBuffer(
            entry.source->cycleEnvelopePlaybackView(),
            sampleCount,
            normalizedTimeIncrement,
            voiceIndex,
            entry.props,
            1.f);
    entry.values[(size_t) laneIndex] = entry.playback.sustainLevel(voiceIndex);
}

PreparedCycleEnvelopeBank::Entry* PreparedCycleEnvelopeBank::entryFor(
        int bufferIndex) {
    if (bufferIndex < 0) {
        return nullptr;
    }
    const auto found = std::find_if(
            entries.begin(), entries.end(), [&](const auto& entry) {
                return entry->bufferIndex == bufferIndex;
            });
    return found == entries.end() ? nullptr : found->get();
}

const PreparedCycleEnvelopeBank::Entry* PreparedCycleEnvelopeBank::entryFor(
        int bufferIndex) const {
    if (bufferIndex < 0) {
        return nullptr;
    }
    const auto found = std::find_if(
            entries.begin(), entries.end(), [&](const auto& entry) {
                return entry->bufferIndex == bufferIndex;
            });
    return found == entries.end() ? nullptr : found->get();
}

}
