#include <algorithm>
#include <limits>

#include "Runtime/VoiceNoteState.h"

namespace CycleV2 {

void VoiceNoteState::reset() {
    held.fill(0);
    order.fill(0);
    nextOrder = 0;
    remainingSamples = 0;
    increment = 0.f;
    target = pitch;
}

void VoiceNoteState::start(int note) {
    reset();
    pitch = (float) note;
    target = pitch;
    press(note, 0);
}

void VoiceNoteState::press(int note, size_t glideSamples) {
    if (note < 0 || note >= (int) held.size()) {
        return;
    }
    const size_t index = (size_t) note;
    if (held[index] == std::numeric_limits<uint16_t>::max()) {
        return;
    }
    ++held[index];
    order[index] = ++nextOrder;
    if (target != (float) note) {
        retarget(note, glideSamples);
    }
}

bool VoiceNoteState::release(int note, size_t glideSamples) {
    if (note < 0 || note >= (int) held.size() || held[(size_t) note] == 0) {
        return false;
    }
    const int previous = currentNote();
    --held[(size_t) note];
    const int current = currentNote();
    if (current >= 0 && current != previous) {
        retarget(current, glideSamples);
    }
    return true;
}

int VoiceNoteState::currentNote() const {
    int result = -1;
    uint64_t newest {};
    for (size_t index = 0; index < held.size(); ++index) {
        if (held[index] > 0 && order[index] > newest) {
            newest = order[index];
            result = (int) index;
        }
    }
    return result;
}

void VoiceNoteState::abandon(std::array<uint16_t, 128>& pendingNoteOffs) {
    for (size_t index = 0; index < held.size(); ++index) {
        pendingNoteOffs[index] = (uint16_t) std::min(
                (unsigned) std::numeric_limits<uint16_t>::max(),
                (unsigned) pendingNoteOffs[index] + held[index]);
    }
    reset();
}

void VoiceNoteState::retarget(int note, size_t glideSamples) {
    target = (float) note;
    remainingSamples = glideSamples;
    increment = glideSamples > 0 ? (target - pitch) / (float) glideSamples : 0.f;
    if (glideSamples == 0) {
        pitch = target;
    }
}

void VoiceNoteState::renderPitch(Buffer<float> output) {
    const int rampSamples = (int) std::min((size_t) output.size(), remainingSamples);
    if (rampSamples > 0) {
        output.section(0, rampSamples).ramp(pitch, increment);
        pitch += increment * (float) rampSamples;
        remainingSamples -= (size_t) rampSamples;
    }
    if (remainingSamples == 0) {
        pitch = target;
    }
    if (rampSamples < output.size()) {
        output.section(rampSamples, output.size() - rampSamples).set(pitch);
    }
}

}
