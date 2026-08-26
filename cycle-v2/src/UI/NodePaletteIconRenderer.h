#pragma once

#include <JuceHeader.h>

#include "UI/NodePalette.h"

namespace CycleV2 {

class NodePaletteIconRenderer {
public:
    static void paint(
            Graphics& graphics,
            PaletteIcon icon,
            Rectangle<float> area,
            bool active);
};

}
