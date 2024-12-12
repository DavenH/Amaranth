#pragma once

#include "JuceHeader.h"

#include "HSlider.h"

class MorphSlider :
		public HSlider {
public:
	int dim;
	MorphSlider(SingletonRepo* repo, const String& name, const String& message, bool horizontal = true);
	void paint(Graphics& g) override;
	void paintSpecial(Graphics& g);
	void mouseDrag(const MouseEvent& e) override;
	void mouseDown(const MouseEvent& e) override;
	void mouseEnter(const MouseEvent& e) override;

	void setDepth(const MouseEvent& e);

private:
	Image eye;
	Colour colour;

	JUCE_LEAK_DETECTOR(MorphSlider)
};