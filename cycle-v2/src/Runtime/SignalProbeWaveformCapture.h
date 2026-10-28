#pragma once

#include "Runtime/GraphAudioExecutor.h"

namespace CycleV2 {

// Private blockwise audition of a probe; never reads or advances live voice state.
class SignalProbeWaveformCapture {
public:
    static std::shared_ptr<const std::vector<float>> capture(
            const GraphExecutionPlan& plan,
            const CompiledSignalProbe& address,
            AudioVoiceContext voice,
            double releaseDuration,
            double noteOffTime);
};

}
