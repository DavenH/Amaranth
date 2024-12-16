#pragma once

#include "JuceHeader.h"

class Bounded {
public:
	virtual ~Bounded() = default;

	virtual void setBounds(int x, int y, int width, int height) = 0;
	virtual const Rectangle<int> getBounds() = 0;
	virtual int getX() = 0;
	virtual int getY() = 0;
	virtual int getWidth() = 0;
	virtual int getHeight() = 0;
};

