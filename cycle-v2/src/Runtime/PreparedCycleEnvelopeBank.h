#pragma once

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
    void applyLifecycleEvent(const NoteLifecycleEvent& event);
    void advanceLane(int laneIndex, int sampleCount, double normalizedTimeIncrement);
    void advanceAll(int sampleCount, double normalizedTimeIncrement);

    bool contains(int bufferIndex) const;
    float value(int bufferIndex, int laneIndex = -1) const;
    bool hasPitchEnvelope() const;
    float pitchValue(int laneIndex) const;

private:
    struct Entry {
        int bufferIndex { -1 };
        int laneCount { 1 };
        bool pitch {};
        const CycleEnvelopePlaybackSource* source {};
        const EnvelopeConfiguration* adoptedConfiguration {};
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
            double normalizedTimeIncrement);
    Entry* entryFor(int bufferIndex);
    const Entry* entryFor(int bufferIndex) const;

    std::vector<std::unique_ptr<Entry>> entries;
    int pitchEntryIndex { -1 };
};

}
