#include <UI/MiscGraphics.h>
#include <Util/Arithmetic.h>

#include "RatingSlider.h"
#include "../Dialogs/PresetPage.h"

RatingSlider::RatingSlider(float rating, int row, PresetPage* page) : rating(rating), row(row), page(page) {
    setRepaintsOnMouseActivity(true);
}


RatingSlider::~RatingSlider() {
    // TODO Auto-generated destructor stub
}


void RatingSlider::paint(Graphics& g) {
    int yOffset = 1;
	int height = getHeight() - 2 * yOffset;
	int width = getWidth();

	g.setColour(Colours::black.withAlpha(0.4f));

	for(int i = 0; i < height / 2; ++i)
		g.drawHorizontalLine(i * 2 + 1, 0, width);

	int start = width * (rating / 10.0);
	NumberUtils::constrain<int>(start, 0, width - 1);

	g.setColour(Colour(255, 190, 0).withAlpha(0.3f));

	for(int i = 0; i < height / 2; ++i)
		g.drawHorizontalLine(i * 2 + 1, 0, start - 1);
//	g.fillRect(0, yOffset, start - 1, height);
}


void RatingSlider::resized() {
}


void RatingSlider::setRating(float rating) {
    this->rating = rating;
}


void RatingSlider::setRow(int row) {
    this->row = row;
}


void RatingSlider::mouseDown(const MouseEvent& e) {
    rating = jlimit(0.f, 10.f, 10.f * e.x / float(getWidth()));
}


void RatingSlider::mouseDrag(const MouseEvent& e) {
    rating = jlimit(0.f, 10.f, 10.f * e.x / float(getWidth()));
    repaint();
}


void RatingSlider::mouseUp(const MouseEvent& e) {
    page->ratingChanged(rating, row);
}

