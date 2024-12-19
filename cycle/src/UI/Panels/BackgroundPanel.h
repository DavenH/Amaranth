#pragma once

#include "JuceHeader.h"
#include "../CycleGraphicsUtils.h"
#include <Definitions.h>

using namespace juce;

class BackgroundPanel : public Component {
public:
    void paint(Graphics& g) {
        getObj(CycleGraphicsUtils).fillBlackground(this, g);
    }
};
