#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class TrimeshPanelHostKind {
    Panel2D,
    Panel3D
};

class TrimeshPanelHostDelegate {
public:
    virtual ~TrimeshPanelHostDelegate() = default;

    virtual void requestTrimeshPanelRepaint() = 0;
    virtual void setTrimeshPanelCursor(
            TrimeshPanelHostKind host,
            const juce::MouseCursor& cursor) = 0;
};

}
