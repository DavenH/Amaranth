#pragma once
#include <vector>
#include <utility>
#include "../Array/Buffer.h"

using std::vector;
using std::pair;

class PeakCounter {
public:
    static vector<int> findZeroCrossings(Buffer<float> samples) {
        if (samples.size() < 2)
            return {};

        vector<int> crossings;
        for (int i = 1; i < samples.size(); ++i) {
            if (samples[i] > 0 != samples[i - 1] > 0)
                crossings.push_back(i);


            if (crossings.size() > 1000)
                return crossings;
        }

        return crossings;
    }

    static vector<int> findInflections(Buffer<float> samples) {
        if (samples.size() < 2)
            return {};

        bool increasing = samples[1] > samples[0];
        bool wasIncreasing = increasing;

        vector<int> inflections;
        inflections.push_back(0);

        for (int i = 1; i < samples.size(); ++i) {
            wasIncreasing = increasing;
            increasing = samples[i] > samples[i - 1];

            if (increasing != wasIncreasing) {
                inflections.push_back(i);

                if (inflections.size() > 1000)
                    return inflections;
            }
        }

        inflections.push_back(samples.size() - 1);

        return inflections;
    }
};
