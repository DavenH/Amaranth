#pragma once

#include <cstdint>
#include <vector>
#include "NumberUtils.h"
#include "../Array/Buffer.h"
#include "../Curve/Vertex2.h"

using std::vector;

class Vertex2;

class Arithmetic {
public:
    static void applyLogMapping(Buffer<float> array, float tension);
    static void applyInvLogMapping(Buffer<float> array, float tension);
    static float logMapping(float tension, float x, bool useOffset = false);
    static float invLogMapping(float tension, float x, bool useOffset = false);

    static int binarySearch(float value, Buffer<float> values) {
        if(values.empty()) {
            return 0;
        }

        int lower = 0;
        int upper = values.size() - 1;
        int index = 0;

        if(value > values.back()) {
            return upper;
        }

        if(value < values.front()) {
            return 0;
        }

        while (upper - lower > 5) {
            index = (upper + lower) / 2;

            if(values[index] < value) {
                lower = index;
            } else {
                upper = index;
            }
        }

        for (int i = lower; i <= upper; ++i) {
            if(values[i] >= value) {
                return i;
            }
        }

        jassertfalse;
        return jlimit(0, values.size() - 1, index);
    }

    static double centsToFrequency(float key, double cents, const Range<int>& range) {
        int midiNote            = getNoteForValue(key, range);
        double freqWithCents    = NumberUtils::noteToFrequency(midiNote, cents);
        double baseFreq         = NumberUtils::noteToFrequency(midiNote, 0);

        return freqWithCents - baseFreq;
    }

    static double centsToFrequencyGraphic(float key, double cents, const Range<int>& range) {
        int midiNote            = getGraphicNoteForValue(key, range);
        double freqWithCents    = NumberUtils::noteToFrequency(midiNote, cents);
        double baseFreq         = NumberUtils::noteToFrequency(midiNote, 0);

        return freqWithCents - baseFreq;
    }

    static bool  isPowerOf2(int number) { return ! (number & (number - 1)); }
    static float calcAdditiveScaling(int numHarmonics);
    static float getRelativePan(float pan, float modPan);
    static float getUnitValueForGraphicNote (int key, const Range<int>& range);
    static float getUnitValueForNote        (int key, const Range<int>& range);
    static int   getGraphicNoteForValue     (float value, const Range<int>& range);
    static int   getNoteForValue            (float value, const Range<int>& range);
    static int   getNextPow2(float fperiod);
    static void  getInputOutputRates(int& inRate, int& outRate, double sampleRateReal);
    static void  getPans(float pan, float& left, float& right);
    static void  polarize(Buffer<float> x);
    static void  unpolarize(Buffer<float> x);
};