#pragma once

#include <utility>
#include <Util/Util.h>

#include "../MiscGraphics.h"
#include "../../App/SingletonRepo.h"
#include "../../App/SingletonAccessor.h"
#include "JuceHeader.h"
#include "../../Definitions.h"

class InsetLabel :
        public Component
    ,   public SingletonAccessor {
public:
    InsetLabel(SingletonRepo* repo, String name, float saturation = 0.f) :
            SingletonAccessor(repo, "InsetLabel")
        ,   name    (std::move(name))
        ,   offsetA (-0.8f)
        ,   offsetB (-0.8f)
        ,   font    (FontOptions(15, Font::bold)) {
        brightColor = Colour::greyLevel(0.28f);
        dimColor    = Colour::greyLevel(0.f);

      #ifdef JUCE_MAC
        offsetA -= 0.2f;
        offsetB -= 0.2f;
      #endif
    }

    void setLabelTitle(const String& name) {
        this->name = name;
    }

    void setOffsets(float a, float b) {
        offsetA = a;
        offsetB = b;
    }

    void setColorHSV(float saturation, float brightness) {
        brightColor = Colour::fromHSV(0.6f, saturation, brightness, 1.f);
        dimColor = Colour::fromHSV(0.6f, saturation, 0.0f, 1.f);
    }

    void paint(Graphics& g) override {
        g.setFont(font);

        Graphics::ScopedSaveState sss(g);

        g.setColour (brightColor);
        getObj(MiscGraphics).drawCentredText(g, getLocalBounds(), name);

        g.addTransform(AffineTransform::translation(offsetA, offsetB));
        g.setColour (dimColor);
        getObj(MiscGraphics).drawCentredText(g, getLocalBounds(), name);
    }

    int getFullSize() {
        return Util::getStringWidth(font, name);
    }

private:
    Colour brightColor, dimColor;
    float offsetA, offsetB;
    String name;
    Font font;
};