#pragma once

#include "JuceHeader.h"

class Bounded {
public:
    virtual void setBounds(int x, int y, int width, int height) = 0;

    virtual void setBounds(const juce::Rectangle<int>& bounds) {
        setBounds(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());
    }
    [[nodiscard]] virtual Rectangle<int> getBounds() const = 0;
    virtual int getX() = 0;
    virtual int getY() = 0;
    virtual int getWidth() = 0;
    virtual int getHeight() = 0;
};

