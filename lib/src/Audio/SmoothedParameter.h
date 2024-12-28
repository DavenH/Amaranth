#pragma once

#include "../Array/Buffer.h"
#include "JuceHeader.h"

class SmoothedParameter
{
public:
    SmoothedParameter();
    explicit SmoothedParameter(float initialValue);

    virtual ~SmoothedParameter() = default;

    void update(int sampleDelta);
    void setValueDirect(float value);
    void maybeApplyRamp(Buffer<float> workBuffer, Buffer<float> dest, float multiplicand = 1.f);

    void updateToTarget();
    void setConvergeSpeed(int halflifeSamples)  { this->halflifeSamples = halflifeSamples; }
    void setSmoothingActivity(bool doesSmooth)  { this->smoothingActive = doesSmooth; }

    bool setTargetValue(float value);
    [[nodiscard]] float getTargetValue() const      { return targetValue; }
    [[nodiscard]] float getCurrentValue() const     { return currentValue; }
    [[nodiscard]] float getPastCurrentValue() const { return pastCurrentValue; }
    [[nodiscard]] bool hasRamp() const;

    float operator*(float value) const { return currentValue * value; }
    float operator+(float value) const { return currentValue + value; }
    float operator-(float value) const { return currentValue - value; }
    float operator/(float value) const { return value == 0.f ? 100.f : currentValue / value; };

    SmoothedParameter& operator=(float value)   { targetValue = value; return *this;    }
    bool operator!=(float value) const          { return currentValue != value;         }

    operator float() const                      { return smoothingActive ? currentValue : targetValue; }

private:
    bool  smoothingActive;
    int   halflifeSamples;
    float targetValue;
    float currentValue;
    float pastCurrentValue;

    JUCE_LEAK_DETECTOR(SmoothedParameter)
};