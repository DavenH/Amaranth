#pragma once
#include "JuceHeader.h"

#if JUCE_MAC
    #include <mach/mach_time.h>
    #include <sys/time.h>
#elif JUCE_WINDOWS
    #include <intrin.h>
#else
    #include <x86intrin.h>
#endif

class MicroTimer {
    uint64_t startTicks, endTicks;
    double iticks;
    uint64_t startCycle, endCycle;

public:
    MicroTimer()
        : startTicks(0)
        , endTicks(0)
        , iticks(1)
        , startCycle(0)
        , endCycle(0)
    {
    }

    void start() {
        startTicks = Time::getHighResolutionTicks();
        startCycle = getCPUCycles();
        // Double read to ensure pipeline flush
        startCycle = getCPUCycles() * 2 - startCycle;
    }

    void stop() {
        endCycle = getCPUCycles();
        endTicks = Time::getHighResolutionTicks();
    }

    double getDeltaTime() {
        return Time::highResolutionTicksToSeconds(endTicks - startTicks);
    }

    uint64_t getClocks() {
        return endCycle - startCycle;
    }

private:
    static uint64_t getCPUCycles() {
#if JUCE_MAC
#if defined(__arm64__)
        uint64_t val;
        // On Apple Silicon, use the system timer register
        asm volatile("mrs %0, cntvct_el0" : "=r" (val));
        return val;
#else
        // On Intel Macs, use RDTSC
        return __rdtsc();
#endif
#elif JUCE_WINDOWS
        return __rdtsc();
#else
        // For other platforms (Linux, etc.) using gcc/clang
        unsigned int lo, hi;
        __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
        return ((uint64_t)hi << 32) | lo;
#endif
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