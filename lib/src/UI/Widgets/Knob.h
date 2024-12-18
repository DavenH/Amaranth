#pragma once

#include "../Layout/IDynamicSizeComponent.h"
#include "../../App/SingletonAccessor.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"
#include "../../Util/StringFunction.h"

using namespace juce;

class Knob:
        public Slider
    ,	public IDynamicSizeComponent
    ,	public SingletonAccessor {
public:
    int id;

    explicit Knob(SingletonRepo* repo);
    Knob(SingletonRepo* repo, float defaultValue);
    Knob(SingletonRepo* repo, int id, String hint, float defaultValue);
    ~Knob() override;

    void resized() override;
    void paint(Graphics & g) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseDown(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    int getId() const					{ return id; }
    int getYDelegate() override 		{ return getY(); }
    int getXDelegate() override 		{ return getX(); }
    [[nodiscard]] int getExpandedSize() const override		{ return expandedSize; }
    [[nodiscard]] int getCollapsedSize() const override		{ return collapsedSize;	}
    int getMinorSize() override 		{ return isCurrentCollapsed ? getCollapsedSize() : getExpandedSize(); }

    void setBoundsDelegate(int x, int y, int w, int h) override { setBounds(x, y, w, h); }
    void setCollapsedSize(int size) 	{ collapsedSize = size; }
    void setColour(Colour color)		{ this->colour = color; }
    void setDefaultValue(float value)	{ this->defaultValue = value; }
    void setDrawValueText(bool doit)	{ this->drawValueText = doit; }
    void setHint(String hint)			{ this->hint = hint; }
    void setId(int id)					{ this->id = id; }
    void setName(const String& name) override 	{ this->name = name; }

    void setDefaults();
    void setStringFunction(const StringFunction& toBoth);
    void setStringFunctions(const StringFunction& toString,
                            const StringFunction& toConsole);

    [[nodiscard]] Rectangle<int> getBoundsInParentDelegate() const override;

    bool isVisibleDlg() const override 			{ return isVisible(); }
    void setVisibleDlg(bool isVisible) override { setVisible(isVisible); }

private:
    float theta;
    float r1;
    float r2;
    float defaultValue;
    float multiplier;

    int expandedSize;
    int collapsedSize;

    bool drawValueText;

    StringFunction consoleString;
    StringFunction valueString;

    String name;
    String hint;
    Colour colour;
    GlowEffect glow;
    Ref<Font> font;

    JUCE_LEAK_DETECTOR(Knob);
};
