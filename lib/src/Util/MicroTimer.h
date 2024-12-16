#pragma once
#include "JuceHeader.h"

#include <ipp.h>

class MicroTimer {
    Ipp64u startTicks, endTicks;
    double iticks;
    Ipp64u startTick, endTick;

public:
    MicroTimer() {
        startTicks = 0;
        endTicks = 0;
        iticks = 1;
        startTick = 0;
        endTick = 0;
    }

    void start() {
        startTicks = Time::getHighResolutionTicks();

        startTick = ippGetCpuClocks();
        startTick = ippGetCpuClocks() * 2 - startTick;
    }

    void stop() {
        endTick = ippGetCpuClocks();
        endTicks = Time::getHighResolutionTicks();
    }

    double getDeltaTime() {
        return Time::highResolutionTicksToSeconds(endTicks - startTicks);
    }

    Ipp64u getClocks() {
        return endTick - startTick;
    }
};

class ScopedTime {
public:
    ScopedTime(MicroTimer& t) : timer(t) {
        timer.start();
    }

    ~ScopedTime() {
        timer.stop();
    }

    JUCE_DECLARE_NON_COPYABLE(ScopedTime);

private:
    MicroTimer& timer;
};