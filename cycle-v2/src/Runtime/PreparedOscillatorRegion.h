#pragma once

#include "NodeDspConfiguration.h"

#include <Array/Buffer.h>

#include <memory>

namespace CycleV2 {

struct GraphExecutionPlan;
struct OscillatorRegionPlan;
struct CompiledVoiceContext;

class PreparedOscillatorRegion {
public:
    virtual ~PreparedOscillatorRegion() = default;
    virtual bool replacesDiagnosticProcessors() const = 0;
    virtual void reset() = 0;
    virtual bool process(
            int midiNote,
            float velocity,
            Buffer<float> pitchEnvelope,
            Buffer<float> left,
            Buffer<float> right) = 0;
};

std::unique_ptr<PreparedOscillatorRegion> prepareOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region,
        const CompiledVoiceContext& context,
        const AudioExecutionSpec& spec,
        int maximumCycleSamples);

bool supportsPreparedOscillatorRegion(
        const GraphExecutionPlan& plan,
        const OscillatorRegionPlan& region);

}
