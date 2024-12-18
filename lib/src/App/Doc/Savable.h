#pragma once

#include "JuceHeader.h"
using namespace juce;

class Savable {
public:
    virtual ~Savable() = default;

    virtual void writeXML(XmlElement* element) const = 0;
    virtual bool readXML(const XmlElement* element) = 0;
};
