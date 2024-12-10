#pragma once

#include "JuceHeader.h"

class Parameter
{
public:
    class Listener {
    public:
        virtual void setParameterValue(int id, float value) = 0;
        virtual float getParameterValue(int id) = 0;
    };

    Parameter(int id)
            : id(id), isAutomable(true), listener(nullptr) {
    }

    Parameter(const String& name, int id, bool isAutomable, Listener* listener)
            : name(name), id(id), isAutomable(isAutomable), listener(listener) {
    }

    virtual ~Parameter() {
        listener = nullptr;
    }

    void setValue(float value) {
        if (listener != nullptr)
            listener->setParameterValue(id, value);
    }

    float getValue() {
        if (listener != nullptr)
            return listener->getParameterValue(id);

        return 0;
    }


	bool isAutomable;
	int id;
	double value;
	juce::String name;

	Listener* listener;
};