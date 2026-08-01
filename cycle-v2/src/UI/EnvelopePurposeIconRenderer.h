#pragma once

#include <JuceHeader.h>

#include "../Nodes/Envelope/EnvelopePurpose.h"

namespace CycleV2 {

class EnvelopePurposeIconRenderer {
public:
    static bool hasIcon(EnvelopePurpose purpose);
    static void paint(
            Graphics& graphics,
            EnvelopePurpose purpose,
            Rectangle<float> area,
            float opacity = 0.94f);
};

}
