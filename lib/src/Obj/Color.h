#pragma once

#include "JuceHeader.h"
using namespace juce;

class Color {
public:
    Color() {
        v[0] = v[1] = v[2] = 0;
        v[3] = 1;
    }

    Color(float r, float g, float b, float alpha = 1) {
        v[0] = r;
        v[1] = g;
        v[2] = b;
        v[3] = alpha;
    }

    explicit Color(float grey) {
        v[0] = grey;
        v[1] = grey;
        v[2] = grey;
        v[3] = 1.f;
    }

    [[nodiscard]] bool isSameHueTo(const Color& c) const {
        return v[0] == c.v[0] && v[1] == c.v[1] && v[2] == c.v[2];
    }

    [[nodiscard]] float alpha() const {
        return v[3];
    }

    bool operator==(const Color& other) const {
        return v[0] == other.v[0] &&
               v[1] == other.v[1] &&
               v[2] == other.v[2] &&
               v[3] == other.alpha();
    }

    Color& operator=(const Color& copy) {
//      memcpy(v, copy.v, sizeof(v) * sizeof(v[0]));

        v[0] = copy.v[0];
        v[1] = copy.v[1];
        v[2] = copy.v[2];
        v[3] = copy.alpha();
        return *this;
    }

    Color operator+(const Color& other) {
        return {v[0] + other.v[0], v[1] + other.v[1], v[2] + other.v[2], v[3] + other.alpha()};
    }

    Color operator*(const float factor) const {
        return {v[0] * factor, v[1] * factor, v[2] * factor, v[3] * factor};
    }

    [[nodiscard]] Color withAlpha(float alpha) const {
        return {v[0], v[1], v[2], alpha};
    }

    Colour toColour() const {
        return {uint8(v[0] * 255), uint8(v[1] * 255), uint8(v[2] * 255), v[3]};
    }

    /* ----------------------------------------------------------------------------- */

    float v[4];
private:
    JUCE_LEAK_DETECTOR(Color)
};
