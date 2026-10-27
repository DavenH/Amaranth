#include "UI/PanelPlaybackOverlay.h"

namespace CycleV2 {

bool PanelPlaybackOverlay::setFrame(const PlaybackCursorFrame& next) {
    bool changed = frame.nodeKey != next.nodeKey || frame.count != next.count;
    for (size_t index = 0; !changed && index < next.count; ++index) {
        const auto& previous = frame.voices[index];
        const auto& current = next.voices[index];
        changed = previous.morph != current.morph || previous.envelopeTime != current.envelopeTime
                || previous.hasMorph != current.hasMorph
                || previous.hasEnvelopeTime != current.hasEnvelopeTime;
    }
    if (changed) {
        frame = next;
    }
    return changed;
}

bool PanelPlaybackOverlay::setSlice(float position) {
    if (slice == position) {
        return false;
    }
    slice = position;
    return true;
}

std::array<PanelPlaybackOverlay::Line, 2 * PlaybackCursorFrame::capacity>
PanelPlaybackOverlay::lines(int axis, bool envelope, bool surface, size_t& count) const {
    std::array<Line, 2 * PlaybackCursorFrame::capacity> result;
    count = 0;
    const size_t morphIndex = axis == Vertex::Red ? 1 : axis == Vertex::Blue ? 2 : 0;
    for (size_t index = 0; index < frame.count; ++index) {
        const auto& voice = frame.voices[index];
        if (envelope && !surface && voice.hasEnvelopeTime) {
            result[count++] = { voice.envelopeTime, false };
        }
        if (surface && voice.hasMorph) {
            result[count++] = { voice.morph[morphIndex], envelope };
        }
    }
    if (surface && frame.count == 0) {
        result[count++] = { slice, envelope };
    }
    return result;
}

std::array<Point<float>, PlaybackCursorFrame::capacity>
PanelPlaybackOverlay::dots(int axis, size_t& count) const {
    std::array<Point<float>, PlaybackCursorFrame::capacity> result;
    count = 0;
    const size_t morphIndex = axis == Vertex::Red ? 1 : axis == Vertex::Blue ? 2 : 0;
    for (size_t index = 0; index < frame.count; ++index) {
        const auto& voice = frame.voices[index];
        if (voice.hasEnvelopeTime && voice.hasMorph) {
            result[count++] = { voice.envelopeTime, voice.morph[morphIndex] };
        }
    }
    return result;
}

void PanelPlaybackOverlay::draw(Panel& panel, int axis, bool envelope, bool surface) const {
    auto* renderer = panel.getPanelRenderer();
    if (renderer == nullptr) {
        return;
    }
    size_t count {};
    const auto cursors = lines(axis, envelope, surface, count);
    renderer->setCurrentLineWidth(1.f);
    renderer->setCurrentColour(0.85f, 0.88f, 0.92f, envelope && surface ? 0.4f : 0.8f);
    for (size_t index = 0; index < count; ++index) {
        const auto& line = cursors[index];
        const float coordinate = line.horizontal ? panel.sy(line.position) : panel.sx(line.position);
        const float limit = (float) (line.horizontal ? panel.getHeight() : panel.getWidth());
        if (coordinate >= 0.f && coordinate <= limit) {
            renderer->drawLine(line.horizontal ? 0.f : coordinate,
                    line.horizontal ? coordinate : 0.f,
                    line.horizontal ? (float) panel.getWidth() : coordinate,
                    line.horizontal ? coordinate : (float) panel.getHeight(), false);
        }
    }
    if (envelope && surface) {
        const auto markers = dots(axis, count);
        renderer->enableSmoothing();
        for (size_t index = 0; index < count; ++index) {
            const Vertex2 point(panel.sx(markers[index].x), panel.sy(markers[index].y));
            if (point.x >= 0.f && point.x <= panel.getWidth()
                    && point.y >= 0.f && point.y <= panel.getHeight()) {
                renderer->setCurrentColour(0.04f, 0.06f, 0.09f, 0.95f);
                renderer->drawPoint(16.f, point, false);
                renderer->setCurrentColour(0.95f, 0.97f, 1.f, 1.f);
                renderer->drawPoint(10.f, point, false);
            }
        }
    }
}

void PanelPlaybackOverlay::appendAutomation(DynamicObject& state,
        int axis, bool envelope, bool surface) const {
    size_t count {};
    const auto cursors = lines(axis, envelope, surface, count);
    Array<var> values;
    for (size_t index = 0; index < count; ++index) {
        auto* cursor = new DynamicObject();
        cursor->setProperty("position", cursors[index].position);
        cursor->setProperty("axis", cursors[index].horizontal ? "y" : "x");
        values.add(cursor);
    }
    state.setProperty("playbackCursors", values);
    state.setProperty("playbackVoiceCount", (int) frame.count);
    Array<var> positions;
    if (envelope && surface) {
        const auto markers = dots(axis, count);
        for (size_t index = 0; index < count; ++index) {
            auto* marker = new DynamicObject();
            marker->setProperty("x", markers[index].x);
            marker->setProperty("y", markers[index].y);
            positions.add(marker);
        }
    }
    state.setProperty("playbackDots", positions);
}

}
