#include "SmoothedParameter.h"

SmoothedParameter::SmoothedParameter() :
        halflifeSamples(128)
    ,   targetValue(0.f)
    ,   currentValue(0.f)
    ,   pastCurrentValue(0.f)
    ,   smoothingActive(true) {
}

SmoothedParameter::SmoothedParameter(float initialValue) :
        halflifeSamples(128)
    ,   targetValue(initialValue)
    ,   currentValue(initialValue)
    ,   smoothingActive(true) {
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

    float product = 0.f;
    float base = 0.5f;
    float expt = deltaSamples / (double) halflifeSamples;

    product = std::pow(base, expt);

    jassert(product <= 1);

    currentValue += (1 - product) * (targetValue - currentValue);

    if (fabsf(currentValue - targetValue) < 0.0001f) {
        currentValue = targetValue;
        pastCurrentValue = targetValue;
    }
}

void SmoothedParameter::setValueDirect(float value) {
    this->targetValue = value;
    currentValue      = value;
    pastCurrentValue  = value;
}

void SmoothedParameter::maybeApplyRamp(
        Buffer<float> workBuffer,
        Buffer<float> dest,
        float multiplicand) {
    jassert(workBuffer.size() == dest.size());

    if (hasRamp()) {
        float startValue = pastCurrentValue * multiplicand;
        float endVal = currentValue * multiplicand;
        float slope = (endVal - startValue) / float(workBuffer.size());

        workBuffer.ramp(startValue, slope);
        dest.mul(workBuffer);
    } else {
        dest.mul(float(currentValue * multiplicand));
    }
}

bool SmoothedParameter::setTargetValue(float value) {
    float lastVal = targetValue;
    targetValue = value;

    if(! smoothingActive) {
        currentValue = targetValue;
    }

    return lastVal != targetValue;
}

void SmoothedParameter::updateToTarget() {
    currentValue = targetValue;
    pastCurrentValue = targetValue;
}

bool SmoothedParameter::hasRamp() const {
    return smoothingActive ? fabsf(pastCurrentValue - currentValue) > 0.001f : false;
}

