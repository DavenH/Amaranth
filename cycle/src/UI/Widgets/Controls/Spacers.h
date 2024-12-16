#pragma once

#include <UI/Layout/IDynamicSizeComponent.h>
#include <iostream>

using namespace juce;

class ClearSpacer : public IDynamicSizeComponent
{
	bool reducible;
	bool isVisible;
	int space, x, y;
	JUCE_LEAK_DETECTOR(ClearSpacer)

public:
	ClearSpacer(int space, bool _reducible = true) :
			reducible(_reducible)
		, 	space(space)
		, 	isVisible(true)
		,	x(0)
		,	y(0) {
    }

    ~ClearSpacer() override = default;

    int getExpandedSize() override {
        return space;
    }

    int getCollapsedSize() override {
        return reducible ? 2 : space;
    }

    void setBoundsDelegate(int x, int y, int /*w*/, int /*h*/) override {
        this->x = x;
        this->y = y;
    }

    const Rectangle<int> getBoundsInParentDelegate() {
        return Rectangle<int>(x, y, space, space);
    }

    int getYDelegate() override {
        return y;
    }

    int getXDelegate() override {
        return x;
    }

	bool isVisibleDlg() const override { return isVisible; }
	void setVisibleDlg(bool isVisible) override { this->isVisible = isVisible; }
};

class Separator :
	public IDynamicSizeComponent,
	public Component {
    bool isHorz;
    int x, y;

    JUCE_LEAK_DETECTOR(Separator)

public:
    Separator(bool layout);
    int getExpandedSize();
	int getCollapsedSize();
	void paint(Graphics& g);
	void setBoundsDelegate(int x, int y, int w, int h);
	const Rectangle<int> getBoundsInParentDelegate();
	int getYDelegate();
	int getXDelegate();
};
