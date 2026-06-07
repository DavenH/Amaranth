#pragma once

#include <JuceHeader.h>

#include "NodeCanvas.h"

namespace CycleV2 {

class NodeWorkspace : public Component {
public:
    NodeWorkspace();

    bool saveGraphToFile(const File& file);
    bool loadGraphFromFile(const File& file);

    void resized() override;

private:
    NodeCanvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeWorkspace)
};

}
