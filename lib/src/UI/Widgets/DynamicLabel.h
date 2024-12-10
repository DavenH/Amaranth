#pragma once

#include "../Layout/IDynamicSizeComponent.h"
#include "../MiscGraphics.h"
#include "../../Obj/Ref.h"

class DynamicLabel :
	public Component,
	public IDynamicSizeComponent
{
public:
	DynamicLabel(MiscGraphics& mg, const String& text)
		: 	text(text)
		,	miscGfx(&mg) {
		setColour(Label::textColourId, Colour::greyLevel(0.55f));
	}

    void paint(Graphics& g) {
		int x = 0;
		int y = 5;
		miscGfx->drawShadowedText(g, text, x, y, *miscGfx->getSilkscreen());
	}

    int getExpandedSize() {
        return 7;
    }

    int getMinorSize() {
        return miscGfx->getSilkscreen()->getStringWidth(text);
    }

    int getCollapsedSize() {
        return miscGfx->getSilkscreen()->getStringWidth(text);
    }

    void setBoundsDelegate(int x, int y, int w, int h) {
        setBounds(x, y, w, h);
    }

    const Rectangle<int> getBoundsInParentDelegate() {
        return getBoundsInParent();
    }

    int getYDelegate() {
        return getY();
    }

    int getXDelegate() {
        return getX();
    }

    void setText(const String& text) {
        this->text = text;
        repaint();
    }

private:
	Ref<MiscGraphics> miscGfx;
	String text;
};
