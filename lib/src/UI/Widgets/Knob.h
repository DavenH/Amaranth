#pragma once

#include "../Layout/IDynamicSizeComponent.h"
#include "../../App/SingletonAccessor.h"
#include "../../Obj/Ref.h"
#include "JuceHeader.h"
#include "../../Util/StringFunction.h"


class Knob:
		public Slider
	,	public IDynamicSizeComponent
	,	public SingletonAccessor {
public:
	int id;

	Knob(SingletonRepo* repo);
	Knob(SingletonRepo* repo, float defaultValue);
	Knob(SingletonRepo* repo, int id, String hint, float defaultValue);
	~Knob();

	void resized();
	void paint(Graphics & g);
	void mouseEnter(const MouseEvent& e);
	void mouseDrag(const MouseEvent& e);
	virtual void mouseDown(const MouseEvent& e);
	void mouseUp(const MouseEvent& e);

	int getId()							{ return id; }
	int getYDelegate() 					{ return getY(); }
	int getXDelegate() 					{ return getX(); }
	int getExpandedSize() 				{ return expandedSize; }
	int getCollapsedSize() 				{ return collapsedSize;	}
	int getMinorSize() 					{ return isCurrentCollapsed ? getCollapsedSize() : getExpandedSize(); }

	void setBoundsDelegate(int x, int y, int w, int h) { setBounds(x, y, w, h); }
	void setCollapsedSize(int size) 	{ collapsedSize = size; }
	void setColour(Colour color)		{ this->colour = color; }
	void setDefaultValue(float value)	{ this->defaultValue = value; }
	void setDrawValueText(bool doit)	{ this->drawValueText = doit; }
	void setHint(String hint)			{ this->hint = hint; }
	void setId(int id)					{ this->id = id; }
	void setName(const String& name) 	{ this->name = name; }

	void setDefaults();
	void setStringFunction(const StringFunction& toBoth);
	void setStringFunctions(const StringFunction& toString,
							const StringFunction& toConsole);

	const Rectangle<int> getBoundsInParentDelegate();

	bool isVisibleDlg() const 			{ return isVisible(); }
	void setVisibleDlg(bool isVisible) 	{ setVisible(isVisible); }

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
