#pragma once

#include "JuceHeader.h"

using namespace juce;

class Parameter
{
public:
    class Listener {
    public:
        virtual void setParameterValue(int id, float value) = 0;
        virtual float getParameterValue(int id) = 0;
    };

    explicit Parameter(int id)
            : id(id), isAutomable(true), listener(nullptr) {
    }

    explicit Parameter(const String& name, int id, bool isAutomable, Listener* listener)
        : name(name), id(id), value(0), isAutomable(isAutomable), listener(listener) {
    }

    virtual ~Parameter() {
        listener = nullptr;
    }

    void setValue(float value) const {
        if (listener != nullptr) {
            listener->setParameterValue(id, value);
        }
    }

    [[nodiscard]] float getValue() const {
        if (listener != nullptr) {
            return listener->getParameterValue(id);
        }

        return 0;
    }

    bool isAutomable;
    int id;
    double value{};
    String name;

    Listener* listener;
};