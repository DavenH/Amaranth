#pragma once

#include <utility>

#include "../Layout/IDynamicSizeComponent.h"
#include "../MiscGraphics.h"
#include "../../Obj/Ref.h"
#include "../../Util/Util.h"

class DynamicLabel :
    public Component,
    public IDynamicSizeComponent
{
public:
    DynamicLabel(MiscGraphics& mg, String  text)
        : 	text(std::move(text))
        ,	miscGfx(&mg) {
        setColour(Label::textColourId, Colour::greyLevel(0.55f));
    }

    void paint(Graphics& g) override {
        int x = 0;
        int y = 5;
        MiscGraphics::drawShadowedText(g, text, x, y, *miscGfx->getSilkscreen());
    }

    [[nodiscard]] int getExpandedSize() const override {
        return jmax(7, getMinorSize());
    }

    [[nodiscard]] int getMinorSize() const override {
        return roundToInt(Util::getStringWidth(*miscGfx->getSilkscreen(), text));
    }

    [[nodiscard]] int getCollapsedSize() const override {
        return 7;
        // return miscGfx->getSilkscreen()->getStringWidth(text);
    }

    void setBoundsDelegate(int x, int y, int w, int h) override {
        setBounds(x, y, w, h);
    }

    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override {
        return getBoundsInParent();
    }

    int getYDelegate() override {
        return getY();
    }

    int getXDelegate() override {
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
