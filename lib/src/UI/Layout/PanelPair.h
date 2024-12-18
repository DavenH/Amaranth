#pragma once
#include <iostream>
#include <climits>
#include "Dragger.h"
#include "Bounded.h"
#include "../../App/SingletonAccessor.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"

class PanelPair :
		public Bounded
	, 	public SingletonAccessor {
public:
	PanelPair(SingletonRepo* repo, Bounded* a, Bounded* b,
			  bool sideBySide, float portion, const String& name,
			  int border = 6, int min1 = 0, int max1 = INT_MAX,
			  int min2 = 0,  int max2 = INT_MAX);

	~PanelPair() override = default;

	void setBounds(int x, int y, int width, int height) override;
	void setDragger(Dragger* dragger);

	void setExactHeight	(int heightOfFirst, int heightOfSecond);
	void setExactWidth	(int widthOfFirst,  int widthOfSecond);
	void setMaxHeight	(int heightOfFirst, int heightOfSecond);
	void setMaxWidth	(int widthOfFirst,  int widthOfSecond);
	void setMinHeight	(int heightOfFirst, int heightOfSecond);
	void setMinWidth	(int widthOfFirst,  int widthOfSecond);

	int getBorder() const 		{ return border; 	}
	int getHeight() override	{ return height; 	}
	int getWidth() override		{ return width; 	}
	int getX() override			{ return x; 		}
	int getY() override			{ return y; 		}
	float getPortion() const	{ return portion; 	}
	const Rectangle<int> getBounds() override { return Rectangle(x, y, width, height); }

	void setFirst(Bounded* component) 	{ one = component; }
	void setSecond(Bounded* component)	{ two = component; }
	void setPortion(float value) 		{ portion = value; }
	void resized() 						{ setBounds(x, y, width, height); }

	bool sideBySide;

	int minWidthOne, minHeightOne, minWidthTwo, minHeightTwo;
	int maxWidthOne, maxHeightOne, maxWidthTwo, maxHeightTwo;

	Bounded* one;
	Bounded* two;

protected:
	bool childrenEnabled;
	int border, x, y, width, height;
	float portion;

	Rectangle<int> hitboxRectangle;
	Ref<Dragger> dragger;

private:
	JUCE_LEAK_DETECTOR(PanelPair)
};