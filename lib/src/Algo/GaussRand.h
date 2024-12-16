#pragma once

#include <ipps.h>
#include "JuceHeader.h"
#include "../Array/Buffer.h"

class GaussRand {
public:
    GaussRand() {
        init(0, 2, Time::getApproximateMillisecondCounter());
    }

    GaussRand(float average, float stdDev, int seed) {
        init(average, stdDev, seed);
    }

    void fill(Buffer<float> buffer) {
        ippsRandGauss_32f(buffer, buffer.size(), randState);
    }

    ~GaussRand() {
        ippsFree(randState);
        randState = nullptr;
    }

private:
    void init(float average, float stdDev, int seed) {
        int stateSize;
        ippsRandGaussGetSize_32f(&stateSize);
        randState = (IppsRandGaussState_32f*) ippsMalloc_8u(stateSize);
        ippsRandGaussInit_32f(randState, average, stdDev, seed);
    }

    IppsRandGaussState_32f* randState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GaussRand);
};
