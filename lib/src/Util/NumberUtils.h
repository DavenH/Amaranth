#pragma once
#include <cmath>
#include "JuceHeader.h"

using namespace juce;

class NumberUtils {
public:
    template<typename type>
    static void constrain(type& n, type lowerBound, type upperBound) {
        if (n < lowerBound)
            n = lowerBound;
        if (n > upperBound)
            n = upperBound;
    }

    template<typename T>
    static void constrain(T& n, const Range <T>& range) {
        if (n < range.getStart()) {
            n = range.getStart();
        }
        if (n > range.getEnd()) {
            n = range.getEnd();
        }
    }

    template<typename T>
    static bool within(T a, const Range <T>& range) {
        return range.contains(a);
    }

    template<typename T>
    static bool within(T a, T lower, T upper) {
        return a >= lower && a <= upper;
    }

    template<typename T>
    static bool withinExclusive(T a, T lower, T upper) {
        return a > lower && a < upper;
    }

    template<typename T>
    static bool withinExclUpper(T a, T lower, T upper) {
        return a >= lower && a < upper;
    }

    static int log2i(unsigned x) {
        if (x == 0) {
            return 0;
        }

        unsigned count = 32;
        while (!(x & (1 << --count)))
            ;
        return count;
    }

    static int nextPower2(unsigned x) {
        if (x == 0) {
            return 0;
        }

        unsigned result = 1;
        while (result < x) {
            result <<= 1;
        }

        return result;
    }

    static double toDecibels(double value) {
        return 6. * log(fabs(value)) / log(2.);
    }

    static double fromDecibels(double dbs) {
        return exp(dbs * log(2.) / 6.);
    }

    static int periodToNote(float period, float samplerate) {
        return frequencyToNote(samplerate / period);
    }

    static double noteToFrequency(int midiNote, double cents = 0.) {
        static const int midiA = 12 * 6 + 9;
        return 440. * pow(2., (midiNote + cents * 0.01 - midiA) / 12.0);
    }

    static double frequencyToNote(float freq) {
        return 12. * (log(freq / 440.) / log(2.) + 6.) + 9.;
    }

    static double unitPitchToSemis(double unitPitch) {
        return 12 * (unitPitch * 2. - 1.);
    }

    static double semisToUnitPitch(double semis) {
        return semis / 24.0 + 0.5;
    }

};
