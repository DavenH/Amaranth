#pragma once

#include "../Array/Buffer.h"
#include "JuceHeader.h"

class SmoothedParameter
{
public:
    SmoothedParameter();
    SmoothedParameter(double initialValue);

    virtual ~SmoothedParameter() {}

    void update(int sampleDelta);
    void setValueDirect(double value);
    void applyRampOrMultiplyApplicably(Buffer<float> workBuffer, Buffer<float> dest, double multiplicand = 1);

    void updateToTarget();
    void setConvergeSpeed(int halflifeSamples)	{ this->halflifeSamples = halflifeSamples; }
    void setSmoothingActivity(bool doesSmooth) 	{ this->smoothingActive = doesSmooth; }

    bool setTargetValue(double value);
    double getTargetValue() const				{ return targetValue; }
    double getCurrentValue() const				{ return currentValue; }
    double getPastCurrentValue() const			{ return pastCurrentValue; }
    bool hasRamp() const;

    double operator*(double value) const		{ return currentValue * value; }
    double operator+(double value) const		{ return currentValue + value; }
    double operator-(double value) const		{ return currentValue - value; }
    double operator/(double value) const		{ return value == 0.f ? 100.f : currentValue / value; };

    SmoothedParameter& operator=(double value)	{ targetValue = value; return *this;	}
    bool operator!=(double value) const			{ return currentValue != value; 		}

    operator double() const						{ return smoothingActive ? currentValue : targetValue; }

private:
    bool 	smoothingActive;
    int 	halflifeSamples;
    double 	targetValue;
    double 	currentValue;
    double 	pastCurrentValue;

    JUCE_LEAK_DETECTOR(SmoothedParameter)
};