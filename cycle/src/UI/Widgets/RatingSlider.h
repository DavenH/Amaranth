#pragma once

#include "JuceHeader.h"
#include <Obj/Ref.h>

class PresetPage;

class RatingSlider :
	public Component {
public:
	RatingSlider(float, int row, PresetPage* page);

	~RatingSlider() override = default;
	void paint(Graphics& g) override;
	void resized() override;
	void setRating(float rating);
	void setRow(int row);
	void mouseDown(const MouseEvent& e) override;
	void mouseDrag(const MouseEvent& e) override;
	void mouseUp(const MouseEvent& e) override;

private:
	float rating;
	int row;
	Ref<PresetPage> page;

	JUCE_LEAK_DETECTOR(RatingSlider)
};