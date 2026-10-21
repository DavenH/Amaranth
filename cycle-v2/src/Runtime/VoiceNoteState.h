#pragma once

#include <Array/Buffer.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace CycleV2 {

class VoiceNoteState {
public:
    void reset();
    void start(int note);
    void press(int note, size_t glideSamples);
    bool release(int note, size_t glideSamples);
    int currentNote() const;
    void abandon(std::array<uint16_t, 128>& pendingNoteOffs);
    void renderPitch(Buffer<float> output);

private:
    void retarget(int note, size_t glideSamples);

    std::array<uint16_t, 128> held {};
    std::array<uint64_t, 128> order {};
    uint64_t nextOrder {};
    float pitch { 60.f };
    float target { 60.f };
    float increment {};
    size_t remainingSamples {};
};

}
