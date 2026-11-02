#pragma once

#include "Runtime/GraphAudioExecutor.h"

namespace CycleV2 {

// One prepared snapshot's private auditions; never reads or advances live voice state.
class SignalProbeWaveformCapture final {
public:
    using Samples = std::shared_ptr<const std::vector<float>>;
    struct Request {
        CompiledSignalProbe address;
        double releaseDuration {};
        double noteOffTime { 1. };
    };

    std::vector<Samples> capture(
            const GraphExecutionPlan& plan,
            const std::vector<Request>& requests,
            int midiNote,
            const std::array<int, 128>& controllers);
    uint64_t executionCount() const;

private:
    struct Key {
        int sourceStep {};
        int sourceOutput {};
        size_t length {};
        size_t noteOff {};
        int midiNote {};
        std::array<int, 128> controllers {};

        bool operator==(const Key& other) const {
            return sourceStep == other.sourceStep && sourceOutput == other.sourceOutput
                    && length == other.length && noteOff == other.noteOff
                    && midiNote == other.midiNote && controllers == other.controllers;
        }
    };
    struct Entry {
        Key key;
        Samples samples;
    };

    mutable juce::CriticalSection lock;
    std::vector<Entry> entries;
    uint64_t executions {};
};

}
