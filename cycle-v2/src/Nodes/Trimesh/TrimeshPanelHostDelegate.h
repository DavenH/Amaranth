#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class TrimeshPanelHostDelegate {
public:
    virtual ~TrimeshPanelHostDelegate() = default;

    virtual void requestTrimeshPanelRepaint() = 0;
    virtual void setTrimeshPanelCursor(const juce::MouseCursor& cursor) = 0;
    virtual void handleMouseOutsideTrimeshPanels(juce::Point<float> screenPosition) = 0;
};

}
