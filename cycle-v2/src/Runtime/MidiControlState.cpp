#include "MidiControlState.h"

namespace CycleV2 {

void MidiControlState::prepare(size_t maximumEventsPerChannel) {
    eventCapacity = maximumEventsPerChannel;
    for (auto& channel : channels) {
        channel.events.reserve(eventCapacity);
    }
}

void MidiControlState::prepareVoice(AudioVoiceContext& voice) const {
    voice.controlEvents.reserve(eventCapacity);
}

void MidiControlState::beginBlock() {
    for (auto& channel : channels) {
        channel.blockStartPressure = channel.pressure;
        channel.blockStartControllers = channel.controllers;
        channel.events.clear();
    }
}

void MidiControlState::ingest(const MidiMessage& message, size_t sampleOffset) {
    ChannelState& channel = channels[(size_t) channelIndex(message.getChannel())];
    if (message.isController()) {
        const int controller = jlimit(0, 127, message.getControllerNumber());
        const float value = (float) message.getControllerValue() / 127.f;
        channel.controllers[(size_t) controller] = value;
        if (channel.events.size() < eventCapacity) {
            channel.events.push_back({
                    ControlEventKind::Controller,
                    sampleOffset,
                    controller,
                    value
            });
        }
    } else if (message.isChannelPressure()) {
        const float value = (float) message.getChannelPressureValue() / 127.f;
        channel.pressure = value;
        if (channel.events.size() < eventCapacity) {
            channel.events.push_back({
                    ControlEventKind::ChannelPressure,
                    sampleOffset,
                    0,
                    value
            });
        }
    }
}

void MidiControlState::populateVoice(AudioVoiceContext& voice, int midiChannel) const {
    const ChannelState& channel = channels[(size_t) channelIndex(midiChannel)];
    voice.controls.channelPressure = channel.blockStartPressure;
    voice.controls.controllers = channel.blockStartControllers;
    jassert(voice.controlEvents.capacity() >= eventCapacity);
    if (voice.controlEvents.capacity() < channel.events.size()) {
        voice.controlEvents.clear();
        return;
    }
    voice.controlEvents.assign(channel.events.begin(), channel.events.end());
}

void MidiControlState::reset() {
    for (auto& channel : channels) {
        channel.pressure = 0.f;
        channel.blockStartPressure = 0.f;
        channel.controllers.fill(0.f);
        channel.blockStartControllers.fill(0.f);
        channel.events.clear();
    }
}

int MidiControlState::channelIndex(int midiChannel) {
    return jlimit(1, channelCount, midiChannel) - 1;
}

}
