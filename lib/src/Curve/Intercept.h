#pragma once

#include "VertCube.h"
#include <vector>

using std::vector;

struct Intercept {
    ~Intercept() = default;

    Intercept() {
    }

    Intercept(float xx, float yy, VertCube* cube = 0, float sharpness = 0) :
            x(xx), y(yy), cube(cube), shp(sharpness) {
    }

    Intercept(const Intercept& other) {
        operator=(other);
    }

    void assignWithTranslation(const Intercept& icpt, float diffX) {
        *this = icpt;
        x += diffX;
    }

    void assignBack(vector<Intercept>& backIcpts, int j) {
        int sz = (int) backIcpts.size();
        int o = (j - 1) / sz + 1;
        int i = j - sz * (o - 1) - 1;

        *this = backIcpts[i];
        x += o;
    }

    void assignFront(vector<Intercept>& icpts, int j) {
        int sz = (int) icpts.size();
        int o = (j - 1) / sz + 1;
        int i = sz - 1 - (j - 1 - (o - 1) * sz);

        *this = icpts[i];
        x -= o;
    }

    bool operator<(const Intercept& other) const {
        return x < other.x;
    }

    bool operator==(const Intercept& other) const {
        return x == other.x && y == other.y;
    }

    Intercept& operator+=(const Intercept& other) {
        x += other.x;
        y += other.y;
        shp += other.shp;

        return *this;
    }

    Intercept& operator=(const Intercept& other) {
        x = other.x;
        y = other.y;
        cube = other.cube;
        shp = other.shp;
        adjustedX = other.adjustedX;
        padBefore = other.padBefore;
        padAfter = other.padAfter;
        isWrapped = other.isWrapped;

        return *this;
    }

    bool padBefore{}, padAfter{}, isWrapped{};
    float x{}, y{}, shp{}, adjustedX{};
    VertCube* cube{};

    JUCE_LEAK_DETECTOR(Intercept);
};