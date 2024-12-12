#pragma once

#include "JuceHeader.h"

using namespace juce;

class BackgroundPanel : public Component {
public:
    void paint(Graphics& g) {
        getObj(CycleGraphicsUtils).fillBlackground(this, g);
    }
};
