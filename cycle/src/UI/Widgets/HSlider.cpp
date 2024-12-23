#include <App/SingletonRepo.h>
#include <UI/MiscGraphics.h>
#include <Util/Arithmetic.h>
#include <Util/StringFunction.h>
#include <UI/IConsole.h>
#include <Obj/Color.h>

#include "HSlider.h"

#include <Definitions.h>

#include <utility>
#include <Util/Util.h>

#include "../CycleGraphicsUtils.h"
HSlider::HSlider(SingletonRepo* repo, const String& name, String  message, bool horizontal) :
        Slider			(name)
    ,	SingletonAccessor(repo, name)
    ,	name			(name)
    ,	message			(std::move(message))
    ,	hztl			(horizontal)
    ,	currentValue	(0.f)
    ,	usesRightClick	(false)
    ,	drawsValue		(false)
    ,	valueUpdater	(*this) {
    setMouseCursor(hztl ? MouseCursor::LeftRightResizeCursor : MouseCursor::UpDownResizeCursor);
    setTextBoxStyle(NoTextBox, true, 0, 0);
    setSliderStyle(hztl ? LinearHorizontal : LinearVertical);
    setWantsKeyboardFocus(false);

    colour = Color(1.f, 0.75f, 0.26f).toColour();
}

void HSlider::setColour(Colour c) {
    colour = c;
}

const String& HSlider::getName() {
    return name;
}

void HSlider::setName(const String& name) {
    this->name = name;
    repaint();
}

void HSlider::paintSecond(Graphics& g) {
    int width = getWidth();
    int height = getHeight();
    int maxSize = hztl ? width : height;

    int start = getSliderPosition();
    NumberUtils::constrain<int>(start, 0, maxSize - 1);

    ColourGradient gradient;
    gradient.addColour(0.0f, 	colour.withAlpha(0.85f));
    gradient.addColour(0.25f, 	colour.withAlpha(0.48f));
    gradient.addColour(0.75f, 	colour.withAlpha(0.48f));
    gradient.addColour(1.0f, 	colour.withAlpha(0.85f));

    gradient.isRadial = false;
    gradient.point1 = Point<float>(0.f, 0.f);
    gradient.point2 = Point<float>(hztl ? 0.f : getWidth(), hztl ? getHeight() : 0);

    g.setGradientFill(gradient);
    g.setOpacity(0.5f);

    Rectangle<int> fillR(getLocalBounds().reduced(2, 2));

    g.fillRect(hztl ?
            fillR.removeFromLeft(start) :
            fillR.removeFromBottom(getHeight() - start));

    Font* silkscreen = getObj(MiscGraphics).getSilkscreen();

    g.setFont(*silkscreen);
    g.setOpacity(1.f);
    g.setColour(Colour::greyLevel(hztl ? 0.6f : 0.65f));

    if(hztl) {
        int width = Util::getStringWidth(*silkscreen, name);
        g.drawSingleLineText(name, getWidth() - width - 4, getHeight() / 2 + 3);
    } else {
        AffineTransform transform(AffineTransform::rotation(2*3.1415926535).translated(getWidth() / 2 - 2, 5));
        Graphics::ScopedSaveState sss(g);

        g.addTransform(transform);
        g.drawSingleLineText(name, 0, 0, Justification::left);
    }

    g.setColour(Colours::black);
    Rectangle<int> r(0, 0, getWidth() - 1, getHeight() - 1);
    getObj(MiscGraphics).drawCorneredRectangle(g, r);


    if(drawsValue) {
        g.setColour(currentValue > 1.f ? Colour(0.95f, 0.75f, 0.35f, 1.0f) : Colours::grey);

        float drawnValue = jlimit(0.f, 1.f, currentValue);

        if (hztl) {
            float left = width * drawnValue + 0.5f;

            g.fillRect(left, 2.f, 2.f, float(height) - 4.f);
        } else {
            float top = (height - 1) * drawnValue + 1.5f;

            g.fillRect(2.f, height + 1 - top, float(width) - 4.f, 2.f);
        }
    }
}

void HSlider::paintFirst(Graphics& g) {
    if (isEnabled()) {
        getObj(CycleGraphicsUtils).fillBlackground(this, g);

        if (hztl) {
            for (int i = 0; i < getHeight() / 2 + 1; ++i) {
                g.setColour(Colours::black);
                g.drawHorizontalLine(i * 2, 0, getWidth());
            }
        } else {
            for (int i = 0; i < getWidth() / 2 + 1; ++i) {
                g.setColour(Colours::black);
                g.drawVerticalLine(i * 2, 0, getHeight());
            }
        }
    }
}

int HSlider::getSliderPosition() {
    return hztl ?
            int(getWidth() * (getValue() / getMaximum())) :
            int(getHeight() * (1 - getValue() / getMaximum()));
}


void HSlider::paint(Graphics& g) {
    paintFirst(g);
    paintSecond(g);
}


void HSlider::mouseEnter(const MouseEvent& e) {
    Slider::mouseEnter(e);

    String valueText = consoleString.toString(getValue());
    getObj(IConsole).write(valueText + " - " + message);
    getObj(IConsole).setMouseUsage(true, true, true, usesRightClick);
}


void HSlider::mouseMove(const MouseEvent& e) {
    Slider::mouseMove(e);

    String valueText = consoleString.toString(getValue());
    getObj(IConsole).write(valueText + " - " + message);
}


void HSlider::mouseDown(const MouseEvent& e) {
    if (!e.mods.isRightButtonDown())
        Slider::mouseDown(e);
}


void HSlider::mouseDrag(const MouseEvent& e) {
    if (!e.mods.isRightButtonDown())
        Slider::mouseDrag(e);

    String valueText = consoleString.toString(getValue());
    getObj(IConsole).write(valueText + " - " + message);
}


void HSlider::setStringFunctions(const StringFunction& toString, const StringFunction& toConsole) {
    valueString = toString;
    consoleString = toConsole;
}

void HSlider::setStringFunction(const StringFunction& toString, const String& postString) {
    valueString = toString;
    consoleString = toString.withPostString(postString);
}


void HSlider::setCurrentValue(float value) {
    currentValue = value;
    drawsValue = true;

    valueUpdater.triggerAsyncUpdate();
}


void HSlider::repaintAndResetTimer() {
    repaint();
    stopTimer();
    startTimer(2000);
}


void HSlider::timerCallback() {
    drawsValue = false;
    repaint();
}

