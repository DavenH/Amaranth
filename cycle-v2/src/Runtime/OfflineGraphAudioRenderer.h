#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <JuceHeader.h>

#include "Graph/GraphCompiler.h"
#include "Runtime/RealtimeMidiEventQueue.h"

namespace CycleDsp {
class SpectralStageCaptureSink;
}

namespace CycleV2 {

enum class OfflineGraphAudioRatePolicy {
    Native,
    LegacyInternal44100
};

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
    float outputGain { 0.125f };
    int controlNoteOffset {};
    int64_t randomSeed {};
    bool hasRandomSeed {};
    OfflineGraphAudioRatePolicy ratePolicy { OfflineGraphAudioRatePolicy::Native };
    size_t sampleCount {};
    std::vector<OfflineGraphAudioEvent> events;
    CycleDsp::SpectralStageCaptureSink* spectralStageCapture {};
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
