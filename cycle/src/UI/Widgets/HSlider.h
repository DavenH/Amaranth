#pragma once

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include "JuceHeader.h"
#include <Util/StringFunction.h>

class SingletonRepo;

class HSlider :
		public Slider
	,	public Timer
	,	public SingletonAccessor {
public:
	HSlider(SingletonRepo* repo, const String& name, const String& message, bool horizontal = true);
	~HSlider();

	const String& getName();
	int getSliderPosition();
	void paintFirst(Graphics& g);
	void paintSecond(Graphics& g);
	void repaintAndResetTimer();
	void setColour(Colour c);
	void setCurrentValue(float value);
	void setName(const String& name);
	void timerCallback();

	virtual void paint(Graphics& g);
	virtual void mouseEnter(const MouseEvent& e);
	virtual void mouseMove(const MouseEvent& e);
	virtual void mouseDown(const MouseEvent& e);
	virtual void mouseDrag(const MouseEvent& e);

	void setStringFunction(const StringFunction& toBoth, const String& postString = String::empty);
	void setStringFunctions(const StringFunction& toString,
							const StringFunction& toConsole);

    class ValueAsyncUpdater : public AsyncUpdater {
	public:
		ValueAsyncUpdater(HSlider& slider) : slider(slider) {}

		void handleAsyncUpdate() {
			slider.repaintAndResetTimer();
		}

	private:
		HSlider& slider;
	};

	int id;
	String name, message;
	Colour colour;

	StringFunction consoleString, valueString;

protected:
	bool hztl, usesRightClick, drawsValue;
	float currentValue;

	ValueAsyncUpdater valueUpdater;

	JUCE_LEAK_DETECTOR(HSlider)
};