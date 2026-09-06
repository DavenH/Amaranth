#pragma once

#include <array>
#include <vector>

#include <JuceHeader.h>

#include "Graph/GraphCompiler.h"
#include "Runtime/RealtimeMidiEventQueue.h"

namespace CycleV2 {

struct OfflineGraphAudioEvent {
    size_t sampleOffset {};
    juce::MidiMessage message;
    MidiEventSource source { MidiEventSource::PerformanceKeyboard };
};

struct OfflineGraphAudioRequest {
    double sampleRate { 44100.0 };
    int blockSize { 512 };
    int channelCount { 2 };
    float voiceDurationSeconds { 7.f };
    size_t sampleCount {};
    std::vector<OfflineGraphAudioEvent> events;
};

struct OfflineGraphAudioResult {
    bool succeeded {};
    juce::String error;
    size_t droppedMidiEvents {};
    std::array<std::vector<float>, 2> channels;
};

class OfflineGraphAudioRenderer {
public:
    static OfflineGraphAudioResult render(
            GraphExecutionPlan plan,
            uint64_t revision,
            const OfflineGraphAudioRequest& request);
};

}
