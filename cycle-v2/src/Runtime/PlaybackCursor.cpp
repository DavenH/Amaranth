#include "Runtime/PlaybackCursor.h"

namespace CycleV2 {

void PlaybackCursorExchange::publish(const PlaybackCursorFrame& frame) {
    sequence.fetch_add(1);
    publishedNode.store(frame.nodeKey);
    revision.store(frame.graphRevision);
    count.store(frame.count);
    for (size_t index = 0; index < frame.count; ++index) {
        const auto& source = frame.voices[index];
        auto& destination = voices[index];
        for (size_t axis = 0; axis < source.morph.size(); ++axis) {
            destination.morph[axis].store(source.morph[axis]);
        }
        destination.envelopeTime.store(source.envelopeTime);
        destination.flags.store((source.hasMorph ? 1u : 0u)
                | (source.hasEnvelopeTime ? 2u : 0u));
    }
    sequence.fetch_add(1);
}

PlaybackCursorFrame PlaybackCursorExchange::read() const {
    PlaybackCursorFrame frame;
    const uint64_t before = sequence.load();
    if ((before & 1u) != 0) {
        return frame;
    }
    frame.nodeKey = publishedNode.load();
    frame.graphRevision = revision.load();
    frame.count = count.load();
    for (size_t index = 0; index < frame.count; ++index) {
        auto& destination = frame.voices[index];
        destination.nodeKey = frame.nodeKey;
        for (size_t axis = 0; axis < destination.morph.size(); ++axis) {
            destination.morph[axis] = voices[index].morph[axis].load();
        }
        destination.envelopeTime = voices[index].envelopeTime.load();
        const unsigned flags = voices[index].flags.load();
        destination.hasMorph = (flags & 1u) != 0;
        destination.hasEnvelopeTime = (flags & 2u) != 0;
    }
    return sequence.load() == before ? frame : PlaybackCursorFrame {};
}

}
