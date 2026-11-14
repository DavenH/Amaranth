#pragma once

#include "Runtime/PlaybackCursor.h"

#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelRenderer.h>

namespace CycleV2 {

class PanelPlaybackOverlay {
public:
    bool setFrame(const PlaybackCursorFrame& next);
    bool setSlice(float position);
    bool setKeyScaleAxis(int axis);
    void draw(Panel& panel, int morphAxis, bool envelope, bool surface) const;
    void appendAutomation(DynamicObject& state, int morphAxis, bool envelope, bool surface) const;

private:
    struct Line {
        float position {};
        bool horizontal {};
        bool released {};
    };
    std::array<Line, 2 * PlaybackCursorFrame::capacity> lines(
            int morphAxis, bool envelope, bool surface, size_t& count) const;

    struct Marker {
        Point<float> position;
        bool released {};
    };
    std::array<Marker, PlaybackCursorFrame::capacity> dots(int morphAxis, bool envelope, size_t& count) const;

    PlaybackCursorFrame frame;
    float slice {};
    int keyScaleAxis { -1 };
};

}
