#pragma once

#include <array>
#include <vector>

#include <Array/ScopedAlloc.h>

#include "Graph/GraphCompiler.h"
#include "Runtime/AudioProcessTypes.h"

namespace CycleV2 {

struct EnvelopeConfiguration;

class OscillatorRegionTraversalRenderer {
public:
    void prepare(const CompiledVoiceContext& context, size_t maximumRowCount);
    bool render(SignalTraversalGrid& grid, int midiNote);

private:
    float pitchUnitValue(int laneIndex, size_t column, size_t columnCount) const;

    float voiceDurationSeconds { 1.f };
    double cachedReleaseDuration { -1. };
    std::shared_ptr<const EnvelopeConfiguration> pitchEnvelope;
    CycleDsp::UnisonVoiceLayout layout;
    std::vector<float> pitchEnvelopeUnitValues;
    std::vector<std::vector<float>> pitchEnvelopeUnitValuesByLane;
    ScopedAlloc<float> workspace;
};

}
