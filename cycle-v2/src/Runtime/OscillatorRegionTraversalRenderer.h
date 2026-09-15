#pragma once

#include <array>
#include <vector>

#include <Array/ScopedAlloc.h>

#include "Graph/GraphCompiler.h"
#include "Runtime/AudioProcessTypes.h"

namespace CycleV2 {

class OscillatorRegionTraversalRenderer {
public:
    void prepare(const CompiledVoiceContext& context, size_t maximumRowCount);
    bool render(SignalTraversalGrid& grid, int midiNote);

private:
    float pitchUnitValue(size_t column, size_t columnCount) const;

    float voiceDurationSeconds { 1.f };
    CycleDsp::UnisonVoiceLayout layout;
    std::vector<float> pitchEnvelopeUnitValues;
    ScopedAlloc<float> workspace;
};

}
