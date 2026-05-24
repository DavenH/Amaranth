#pragma once

#include <JuceHeader.h>

#include "NodeCanvas.h"

namespace CycleV2 {

class NodeWorkspace : public Component {
public:
    NodeWorkspace();

    void resized() override;

private:
    NodeCanvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeWorkspace)
};

}
