#pragma once
#include <algorithm>
#include <iostream>
#include <vector>
#include "Color.h"
#include "../Array/ScopedAlloc.h"
#include "JuceHeader.h"

using std::vector;

class ColorGradient {
public:
	ColorGradient();
	void read(Image& image, bool softerAlpha = true, bool isTransparent = false);
	void multiplyAlpha(float alpha);

	Buffer<Ipp8u> getPixels() 		{ return pixels; 					  }
	Buffer<float> getFloatPixels()	{ return floatPixels;				  }

	static float squash(float x)	{ return jmin(1.f, powf((5 * x), 2)); }
	static float squashSoft(float x){ return x < 0.2f ? 5 * x : 1;		  }
	vector<Color>& getColours()		{ return colours;					  }

private:
	int pixelStride;

	vector<Color> colours;
	ScopedAlloc<Ipp8u> pixels;
	ScopedAlloc<float> floatPixels;

	JUCE_LEAK_DETECTOR(ColorGradient);
};