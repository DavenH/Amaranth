#include <ipp.h>

#include "Knob.h"

#include <Util/Util.h>

#include "../IConsole.h"
#include "../MiscGraphics.h"
#include "../../App/SingletonRepo.h"
#include "../../Definitions.h"

Knob::Knob(SingletonRepo* repo) :
        Slider(String())
    , 	SingletonAccessor(repo, "Knob")
{
    setDefaults();

    defaultValue = 0.5f;
    id = 0;
    hint = String();

    setValue(defaultValue, dontSendNotification);
    setWantsKeyboardFocus(false);
}

Knob::Knob(SingletonRepo* repo, int id, String hint, float defaultValue) :
        Slider(hint)
    , 	SingletonAccessor(repo, "Knob")
    , 	id(id)
    ,	hint(hint)
    , 	defaultValue(defaultValue) {
    setDefaults();
    setValue(defaultValue, dontSendNotification);
}

Knob::Knob(SingletonRepo* repo, float defaultValue) :
        Slider(String())
    , 	defaultValue(defaultValue)
    ,	SingletonAccessor(repo, "Knob") {
    setDefaults();

    id = 0;
    hint = String();

    setValue(defaultValue, dontSendNotification);
}

void Knob::setDefaults() {
    setSliderStyle(RotaryVerticalDrag);
    setTextBoxStyle(NoTextBox, true, 0, 0);

    theta 			= 1.2f;
    r1 				= 0.6f;
    r2 				= 0.3f;
    expandedSize 	= 28;
    collapsedSize 	= 24;
    drawValueText 	= true;
    consoleString.withPrecision(2);

    setRange(0, 1);

    colour = Colour::greyLevel(0.65f);
    font = getObj(MiscGraphics).getSilkscreen();
    glow.setGlowProperties(3, Colours::black.withAlpha(0.4f));
}

void Knob::setStringFunctions(const StringFunction& toString,
                              const StringFunction& toConsole) {
    valueString  = toString;
    consoleString = toConsole;
}

void Knob::paint(Graphics& g) {
    float x 		= getWidth() / 2.f;
    float y 		= getHeight() / 2.f;
    float size 		= (float) jmin(getWidth(), getHeight());
    float bigRad 	= size * (r1 + r2) / 2.f;
    float smallRad 	= size * r1 / 2.f;

    bool isSmall 	= size < 28;
    bool isVSmall 	= false;

    ColourGradient darkGrade(Colour::greyLevel(isVSmall ? 0.25f : 0.17f), x, y + 5, colour, -5, -5, true);
    ColourGradient lightGrade(Colour::greyLevel(isVSmall ? 0.95f : 0.8f), x, y, Colour::greyLevel(isVSmall ? 0.6f : 0.3f), 0, 0, true);

    float startX 	= x + bigRad * sin(IPP_PI + theta / 2);
    float startY 	= y - bigRad * cos(IPP_PI + theta / 2);
    float valueAngle= IPP_PI + theta / 2 + getValue() * (2 * IPP_PI - theta);
    float angle 	= (1 - getValue()) * (2 * IPP_PI - theta) + theta / 2;

    Path light;
    light.startNewSubPath(x + smallRad * sinf(angle), y + smallRad * cosf(angle));
    light.lineTo		 (x + bigRad * sinf(angle), y + bigRad * cosf(angle));
    light.addCentredArc	 (x, y, bigRad, bigRad, 0, valueAngle, 3 * IPP_PI - theta / 2);

    angle 	= theta / 2;
    float innerX 	= x + smallRad * sinf(angle);
    float innerY 	= y + smallRad * cosf(angle);

    light.lineTo		(innerX, innerY);
    light.addCentredArc	(x, y, smallRad, smallRad, 0, 3 * IPP_PI - theta / 2, valueAngle);
    light.closeSubPath	();

    Path band;
    band.startNewSubPath(startX, startY);
    band.addCentredArc	(x, y, bigRad, bigRad, 0, IPP_PI + theta / 2, 3 * IPP_PI - theta / 2);
    band.lineTo			(innerX, innerY);
    band.addCentredArc	(x, y, smallRad, smallRad, 0, 3 * IPP_PI - theta / 2, IPP_PI + theta / 2);
    band.lineTo			(startX, startY);
    band.closeSubPath	();

    glow.setGlowProperties(isVSmall ? 1.f : isSmall ? 2.f : 3.f, Colours::black.withAlpha(isVSmall ? 0.5f : 0.3f));

    Image positive(Image::ARGB, getWidth(), getHeight(), true);
    Graphics temp(positive);

    temp.setGradientFill(darkGrade);
    temp.fillPath(band);

    glow.applyEffect(positive, temp, 1.f, 1.f);

    g.drawImageAt(positive, 0, 0);
    g.setGradientFill(lightGrade);
    g.fillPath(light);
    g.setOpacity(1.0f);

    g.setColour(Colour::greyLevel(0.0f));
    g.drawLine(x + smallRad * sinf(angle), y + smallRad * cosf(angle),
               x + bigRad * sinf(angle),	y + bigRad * cosf(angle));

    if (drawValueText) {
        String text = valueString.toString(getValue());
        g.setFont(*font);

        int width = Util::getStringWidth(*font, text);
        g.setColour(Colour::greyLevel(0.6f));
        g.drawSingleLineText(text, (getWidth() - width) / 2, getHeight() / 2 + 3);
    }
}

void Knob::mouseEnter(const MouseEvent& e) {
    setMouseCursor(MouseCursor::UpDownResizeCursor);

    String consoleStr = consoleString.toString(getValue());
    if(hint.isNotEmpty()) {
        consoleStr = consoleStr + " - " + hint;
    }

    repo->getConsole().write(consoleStr);
    repo->getConsole().setMouseUsage(true, true, true, false);
}

void Knob::mouseDrag(const MouseEvent& e) {
    String consoleStr = consoleString.toString(getValue());
    repo->getConsole().write(consoleStr);
    Slider::mouseDrag(e);
}

void Knob::mouseDown(const MouseEvent& e) {
    if (e.mods.isMiddleButtonDown()) {
        setValue(defaultValue);
    } else {
        Slider::mouseDown(e);
    }
}

void Knob::mouseUp(const MouseEvent& e) {
    Slider::mouseUp(e);
}

Rectangle<int> Knob::getBoundsInParentDelegate() const {
    int size = isCurrentCollapsed ? getCollapsedSize() : getExpandedSize();
    return {getX(), getY(), size, size};
}

void Knob::setStringFunction(const StringFunction& toBoth) {
    setStringFunctions(toBoth, toBoth);
}

void Knob::resized() {
    auto size = (float) jmin(getWidth(), getHeight());

    r1 = 0.57f;
    r2 = jmax(4.f / size, 0.22f);
}

