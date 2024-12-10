#include <ipp.h>
#include "SmoothedParameter.h"

SmoothedParameter::SmoothedParameter() :
		halflifeSamples(128)
	,	targetValue(0)
	,	currentValue(0)
	,	pastCurrentValue(0)
	,	smoothingActive(true) {
}

SmoothedParameter::SmoothedParameter(double initialValue) :
		halflifeSamples(128)
	,	targetValue(initialValue)
	,	currentValue(initialValue)
	,	smoothingActive(true) {
}

void SmoothedParameter::update(int deltaSamples) {
    if (!smoothingActive) {
        updateToTarget();
        return;
    }

    if (targetValue == currentValue) {
        pastCurrentValue = currentValue;
        return;
    }

    pastCurrentValue = currentValue;

    double product = 0;
    double base = 0.5;
    double expt = deltaSamples / (double) halflifeSamples;

    ippsPow_64f_A26(&base, &expt, &product, 1);

    jassert(product <= 1);

    currentValue += (1 - product) * (targetValue - currentValue);

    if (fabs(currentValue - targetValue) < 0.0001) {
        currentValue = targetValue;
        pastCurrentValue = targetValue;
	}
}

void SmoothedParameter::setValueDirect(double value) {
	this->targetValue 		= value;
	currentValue 			= value;
	pastCurrentValue 		= value;
}

void SmoothedParameter::applyRampOrMultiplyApplicably(
        Buffer<float> workBuffer,
        Buffer<float> dest,
        double multiplicand) {
    jassert(workBuffer.size() == dest.size());

    if (hasRamp()) {
		double startValue = pastCurrentValue * multiplicand;
		double endVal = currentValue * multiplicand;
		double slope = (endVal - startValue) / double(workBuffer.size());

		workBuffer.ramp(startValue, slope);
        dest.mul(workBuffer);
    } else {
		dest.mul(float(currentValue * multiplicand));
	}
}

bool SmoothedParameter::setTargetValue(double value) {
	double lastVal = targetValue;
	targetValue = value;

	if(! smoothingActive)
		currentValue = targetValue;

	return lastVal != targetValue;
}

void SmoothedParameter::updateToTarget() {
	currentValue = targetValue;
	pastCurrentValue = targetValue;
}

bool SmoothedParameter::hasRamp() const {
    return smoothingActive ? fabs(pastCurrentValue - currentValue) > 0.001 : false;
}

