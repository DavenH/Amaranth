#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <Curve/Rasterization/EnvelopePlaybackEngine.h>

#include "Graph/GraphCompiler.h"
#include "Runtime/AudioProcessTypes.h"

namespace CycleV2 {

class CycleEnvelopePlaybackSource;
class NodeAudioProcessor;
struct EnvelopeConfiguration;

class PreparedCycleEnvelopeBank {
public:
    bool prepare(
            const GraphExecutionPlan& plan,
            const OscillatorRegionPlan& region,
            const std::vector<NodeAudioProcessor*>& processors,
            int laneCount,
            const String& pitchEnvelopeNodeId = {});
    void reset();
    void setVoiceLifecycleSeed(int64_t seed);
    void applyLifecycleEvent(const NoteLifecycleEvent& event);
    void advanceLane(int laneIndex, int sampleCount, double normalizedTimeIncrement,
            PlaybackCursorCapture* cursor = nullptr);
    void advanceAll(int sampleCount, double normalizedTimeIncrement,
            PlaybackCursorCapture* cursor = nullptr);

    bool contains(int bufferIndex) const;
    float value(int bufferIndex, int laneIndex = -1) const;
    bool hasPitchEnvelope() const;
    float pitchValue(int laneIndex) const;

private:
    struct Entry {
        int bufferIndex { -1 };
        uint64_t nodeKey {};
        int laneCount { 1 };
        bool pitch {};
        const CycleEnvelopePlaybackSource* source {};
        const EnvelopeConfiguration* adoptedConfiguration {};
        uint64_t adoptedGeneration {};
        Rasterization::EnvelopeLoopSpan adoptedLoop;
        Rasterization::EnvelopePlaybackEngine playback;
        MeshLibrary::EnvProps props;
        std::vector<float> values;
        std::vector<bool> active;
    };

    static const EnvelopeConfiguration* adopt(Entry& entry);
    static void advance(
            Entry& entry,
            int laneIndex,
            int sampleCount,
            double normalizedTimeIncrement,
            PlaybackCursorCapture* cursor);
    Entry* entryFor(int bufferIndex);
    const Entry* entryFor(int bufferIndex) const;

    std::vector<std::unique_ptr<Entry>> entries;
    int pitchEntryIndex { -1 };
};

}
