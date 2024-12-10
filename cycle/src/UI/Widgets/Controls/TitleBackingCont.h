#pragma once

#include "JuceHeader.h"

class TitleBacking : public Component
{
public:
	void paint(Graphics& g) {
        Rectangle<int> bnds(getLocalBounds());

        g.setGradientFill(ColourGradient(Colour::greyLevel(0.17f),
                                         bnds.getRight() - 10, bnds.getBottom(),
                                         Colour::greyLevel(0.17f).withAlpha(0.f),
                                         bnds.getX(), bnds.getBottom(), true));

        g.fillRect(bnds);
    }
};