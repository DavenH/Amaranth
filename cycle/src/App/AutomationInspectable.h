#pragma once

#include "JuceHeader.h"

class AutomationInspectable {
public:
    virtual ~AutomationInspectable() = default;

    virtual var exportAutomationState() const = 0;
};
