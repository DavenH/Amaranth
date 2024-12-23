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
	explicit ClearSpacer(int space, bool _reducible = true) :
			reducible(_reducible)
		, 	space(space)
		, 	isVisible(true)
		,	x(0)
		,	y(0) {
    }

    ~ClearSpacer() override = default;

    [[nodiscard]] int getExpandedSize() const override {
        return space;
    }

    [[nodiscard]] int getCollapsedSize() const override {
        return reducible ? 2 : space;
    }

    void setBoundsDelegate(int x, int y, int /*w*/, int /*h*/) override {
        this->x = x;
        this->y = y;
    }

    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override {
        return {x, y, space, space};
    }

    int getYDelegate() override {
        return y;
    }

    int getXDelegate() override {
        return x;
    }

	[[nodiscard]] bool isVisibleDlg() const override { return isVisible; }
	void setVisibleDlg(bool isVisible) override { this->isVisible = isVisible; }
};

class Separator :
	public IDynamicSizeComponent,
	public Component {
    bool isHorz;
    int x, y;

    JUCE_LEAK_DETECTOR(Separator)

public:
    explicit Separator(bool layout);
    [[nodiscard]] int getExpandedSize() const override;
	[[nodiscard]] int getCollapsedSize() const override;
	void paint(Graphics& g) override;
	void setBoundsDelegate(int x, int y, int w, int h) override;
	Rectangle<int> getBoundsInParentDelegate() const override;
	int getYDelegate() override;
	int getXDelegate() override;
};
