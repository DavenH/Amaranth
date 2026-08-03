#pragma once

#include <JuceHeader.h>

#include "NodePortVisualResolver.h"

namespace CycleV2 {

class NodePortIconRenderer {
public:
    static bool hasIcon(PortVisualSemantic semantic);
    static void paint(
            Graphics& graphics,
            PortVisualSemantic semantic,
            Rectangle<float> area,
            PortSide side = PortSide::Left,
            float opacity = 0.96f);
};

}
