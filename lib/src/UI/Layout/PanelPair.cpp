#include "PanelPair.h" 
#include "../../App/SingletonRepo.h"

PanelPair::PanelPair(SingletonRepo* repo, Bounded* a, Bounded* b,
					 bool sideBySide, float portion, const String& name,
					 int border, int min1, int max1, int min2, int max2) :
		SingletonAccessor(repo, name)
	,	one			(a)
	,	two			(b)
	,	portion		(portion)
	,	border		(border)
	,	sideBySide	(sideBySide) {
	childrenEnabled = false;
	dragger 		= nullptr;

	minWidthOne 	= sideBySide ? min1 	: 0;
	minHeightOne 	= sideBySide ? 0 		: min1;
	minWidthTwo 	= sideBySide ? min2 	: 0;
	minHeightTwo 	= sideBySide ? 0 		: min2;
	maxWidthOne 	= sideBySide ? max1 	: INT_MAX;
	maxHeightOne 	= sideBySide ? INT_MAX 	: max1;
	maxWidthTwo 	= sideBySide ? max2 	: INT_MAX;
	maxHeightTwo 	= sideBySide ? INT_MAX 	: max2;

	x 				= 0;
	y 				= 0;
	width 			= 0;
	height 			= 0;
}

void PanelPair::setDragger(Dragger* dragger) {
    this->dragger = dragger;
    this->dragger->setPanelPair(this);
}

void PanelPair::setBounds(int x, int y, int width, int height) {
    this->x = x;
    this->y = y;
	this->width = width;
	this->height = height;

	if (sideBySide) {
        int firstWidth, secondWidth;

        firstWidth = roundToInt((width - border) * portion);
        secondWidth = roundToInt((width - border) * (1 - portion));

        if (minWidthOne > firstWidth) {
            firstWidth = minWidthOne;
            secondWidth = (width - border) - firstWidth;
        } else if (minWidthTwo > secondWidth) {
            secondWidth = minWidthTwo;
            firstWidth = (width - border) - secondWidth;
        }

        if (maxWidthOne < firstWidth) {
            firstWidth = maxWidthOne;
            secondWidth = (width - border) - firstWidth;
        } else if (maxWidthTwo < secondWidth) {
            secondWidth = maxWidthTwo;
            firstWidth = (width - border) - secondWidth;
        }

        one->setBounds(x, y, firstWidth, height);
        two->setBounds(x + firstWidth + border, y, secondWidth, height);

        if (dragger) {
            dragger->setBounds(x + firstWidth, y - 2, border + 2, height + 4);
        }

    } else {
        int firstHeight, secondHeight;

        firstHeight = roundToInt((height - border) * portion);
        secondHeight = roundToInt((height - border) * (1 - portion));

        if (minHeightOne > firstHeight) {
            firstHeight = minHeightOne;
            secondHeight = (height - border) - firstHeight;
        } else if (minHeightTwo > secondHeight) {
            secondHeight = minHeightTwo;
            firstHeight = (height - border) - secondHeight;
        }

        if (maxHeightOne < firstHeight) {
            firstHeight = maxHeightOne;
            secondHeight = (height - border) - firstHeight;
        } else if (maxHeightTwo < secondHeight) {
            secondHeight = maxHeightTwo;
            firstHeight = (height - border) - secondHeight;
        }

		one->setBounds(x, y, width, firstHeight);
		two->setBounds(x, y + firstHeight + border, width, secondHeight);

		if (dragger)
			dragger->setBounds(x - 2, y + firstHeight, width + 4, border);
	}
}

void PanelPair::setExactWidth(int widthOfFirst, int widthOfSecond) {
    setMinWidth(widthOfFirst, widthOfSecond);
    setMaxWidth(widthOfFirst, widthOfSecond);
}

void PanelPair::setExactHeight(int heightOfFirst, int heightOfSecond) {
    setMinHeight(heightOfFirst, heightOfSecond);
    setMaxHeight(heightOfFirst, heightOfSecond);
}

void PanelPair::setMinWidth(int widthOfFirst, int widthOfSecond) {
    if (widthOfFirst > 0) minWidthOne = widthOfFirst;
    if (widthOfSecond > 0) minWidthTwo = widthOfSecond;
}

void PanelPair::setMinHeight(int heightOfFirst, int heightOfSecond) {
    if (heightOfFirst > 0) minHeightOne = heightOfFirst;
    if (heightOfSecond > 0) minHeightTwo = heightOfSecond;
}

void PanelPair::setMaxWidth(int widthOfFirst, int widthOfSecond) {
    if (widthOfFirst > 0) maxWidthOne = widthOfFirst;
    if (widthOfSecond > 0) maxWidthTwo = widthOfSecond;
}

void PanelPair::setMaxHeight(int heightOfFirst, int heightOfSecond) {
    if (heightOfFirst > 0) maxHeightOne = heightOfFirst;
    if (heightOfSecond > 0) maxHeightTwo = heightOfSecond;
}
