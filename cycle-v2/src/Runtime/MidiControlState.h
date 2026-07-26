#pragma once

#include <array>

#include <JuceHeader.h>

#include "AudioProcessTypes.h"

namespace CycleV2 {

class MidiControlState {
public:
    static constexpr int channelCount = 16;

    void prepare(size_t maximumEventsPerChannel);
    void prepareVoice(AudioVoiceContext& voice) const;
    void beginBlock();
    void ingest(const MidiMessage& message, size_t sampleOffset);
    void populateVoice(AudioVoiceContext& voice, int midiChannel) const;
    void reset();
    size_t droppedEventCount() const { return droppedEvents; }

private:
    struct ChannelState {
        float pressure {};
        float blockStartPressure {};
        std::array<float, 128> controllers {};
        std::array<float, 128> blockStartControllers {};
        std::vector<TimedControlEvent> events;
    };

    static int channelIndex(int midiChannel);

    std::array<ChannelState, channelCount> channels;
    size_t eventCapacity {};
    size_t droppedEvents {};
};

}
