#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Runtime/PlaybackCursor.h"

namespace CycleV2 {

struct VoicePlaybackPosition {
    uint64_t voiceId {};
    int midiNote {};
    double ageSeconds {};
    double releaseAgeSeconds {};
    double durationSeconds { 1. };
    bool released {};
};

struct VoicePlaybackFrame {
    std::array<VoicePlaybackPosition, PlaybackCursorFrame::capacity> voices;
    size_t count {};
};

// Caller-owned, sorted sample requests within one native audio block. No audio-thread allocation.
struct VoicePlaybackCapture {
    const size_t* samples {};
    VoicePlaybackFrame* frames {};
    size_t count {};
    size_t sampleOrigin {};
    size_t next {};
};

}
