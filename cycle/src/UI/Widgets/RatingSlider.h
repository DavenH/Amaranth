#pragma once

#include "JuceHeader.h"
#include <Obj/Ref.h>

class PresetPage;

class RatingSlider :
	public Component {
public:
	RatingSlider(float, int row, PresetPage* page);
	virtual ~RatingSlider();
	void paint(Graphics& g);
	void resized();
	void setRating(float rating);
	void setRow(int row);
	void mouseDown(const MouseEvent& e);
	void mouseDrag(const MouseEvent& e);
	void mouseUp(const MouseEvent& e);

private:
	float rating;
	int row;
	Ref<PresetPage> page;

	JUCE_LEAK_DETECTOR(RatingSlider)
};