#pragma once

#include <vector>
#include "JuceHeader.h"

using std::vector;

class Dimensions {
public:
    int x;
    int y;
    vector<int> hidden;

    Dimensions() : x(0), y(0) {}

    ~Dimensions() = default;

    Dimensions(int x, int y) : x(x), y(y) {
    }

    Dimensions(int x, int y, int h) : x(x), y(y) {
        hidden.resize(1);
        hidden[0] = h;
    }

    Dimensions(int x, int y, int h1, int h2) : x(x), y(y) {
        hidden.resize(2);
        hidden[0] = h1;
        hidden[1] = h2;
    }

    Dimensions(int x, int y, int h1, int h2, int h3) : x(x), y(y) {
        hidden.resize(3);
        hidden[0] = h1;
        hidden[1] = h2;
        hidden[2] = h3;
    }

    Dimensions(const Dimensions& dims) {
        x = dims.x;
        y = dims.y;
        hidden = dims.hidden;
    }

    [[nodiscard]] int numHidden() const {
        return (int) hidden.size();
    }

    JUCE_LEAK_DETECTOR(Dimensions);
};