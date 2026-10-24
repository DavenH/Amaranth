#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <Obj/MorphPosition.h>

namespace CycleV2 {

// Audio-owned capture for one requested editor and one voice. Never retained by UI.
struct PlaybackCursorCapture {
    uint64_t nodeKey {};
    std::array<float, 3> morph {};
    float envelopeTime {};
    bool hasMorph {};
    bool hasEnvelopeTime {};

    void setMorph(const MorphPosition& position) {
        morph = { position.time, position.red, position.blue };
        hasMorph = true;
    }
};

struct PlaybackCursorFrame {
    static constexpr size_t capacity = 8;
    uint64_t nodeKey {};
    uint64_t graphRevision {};
    std::array<PlaybackCursorCapture, capacity> voices;
    size_t count {};
};

// One audio writer, message-thread readers. Atomic fields keep failed reads race-free.
class PlaybackCursorExchange {
public:
    static_assert(std::atomic<float>::is_always_lock_free);
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    void request(uint64_t nodeKey) { requestedNode.store(nodeKey, std::memory_order_release); }
    uint64_t requested() const { return requestedNode.load(std::memory_order_acquire); }
    void publish(const PlaybackCursorFrame& frame);
    PlaybackCursorFrame read() const;

private:
    struct Voice {
        std::array<std::atomic<float>, 3> morph {};
        std::atomic<float> envelopeTime {};
        std::atomic<unsigned> flags {};
    };

    std::atomic<uint64_t> requestedNode {};
    std::atomic<uint64_t> sequence {};
    std::atomic<uint64_t> publishedNode {};
    std::atomic<uint64_t> revision {};
    std::atomic<size_t> count {};
    std::array<Voice, PlaybackCursorFrame::capacity> voices;
};

}
