#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class TrimeshPanelHostDelegate {
public:
    virtual ~TrimeshPanelHostDelegate() = default;

    virtual void requestTrimeshPanelRepaint() = 0;
};

}
