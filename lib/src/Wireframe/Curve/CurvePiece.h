#pragma once

#include <Array/Buffer.h>
#include <cmath>
#include <iostream>

#include "../Interpolator/Trilinear/TrilinearVertex.h"
#include "../Vertex/Intercept.h"
#include "../Vertex/Vertex2.h"

using std::cout;
using std::endl;
using std::ostream;

class TransformParameters {
public:
    float theta;
    float scaleX;
    float scaleY;
    float shear;
    float sinrot;
    float cosrot;
    int dpole, ypole;
    Vertex2 d;
    friend ostream& operator<<(ostream& stream, TransformParameters t);
};

#ifndef _USE_IPP_AFFINE_TRANSFORM
    #define _USE_IPP_AFFINE_TRANSFORM 0
#endif

class CurvePiece {
public:
    static const int resolutions    = 3u;
    static const int resolution     = 64u;
    static const int numCurvelets   = 128u;

    // the xy arrays that define the parametric path of this CurvePiece
    Float32 transformX[resolution];
    Float32 transformY[resolution];

    // cached table of curve pieces - it's a complex calculation
    // triple nested float array because we have these dimensions:
    // - x-axis
    // - theta (family of curves the set of intersections of a plane and conic section, plane rotated at various theta)
    // - multiple resolutions
    static float*** table;

    // the defining points of the CurvePiece. Each CurvePiece is like a boomerang with
    // the legs at point `a` and `c`, with the vertex at point `b`.
    Intercept a, b, c;

    // The linear algebra intermediates
    TransformParameters tp;

    // Resolution index of the curve, indexes into the table property
    int resIndex;

    // The resolution in samples
    int curveRes;

    // where this CurvePiece is in the series of curve pieces that compose a full curve
    int waveIdx;

    CurvePiece(const Intercept& one, Intercept two, Intercept thr);
    CurvePiece(Vertex2& one, Vertex2& two, Vertex2& thr);
    CurvePiece(const CurvePiece& curve);

    void nullify() {
        waveIdx       = 0;
        tableCurveIdx = 0;
        tableCurvePos = 0;
        resIndex      = 0;
        interpolate   = false;
    }

    bool operator==(const CurvePiece& other) const;
    double function(double x, double t);
    float getCentreX();
    static void calcTable();
    static void deleteTable();
    void construct();
    void destruct();
    void recalculateCurve();
    void recalculatedPadded();
    void setResIndex(int index);
    void setShouldInterpolate(bool should)  { interpolate = should; }
    void update();
    void updateCurrentIndex();
    void validate();

private:
    int tableCurveIdx;
    float tableCurvePos;
    bool interpolate;

    static inline float yValue(int theta, int index, int res);

    JUCE_LEAK_DETECTOR(CurvePiece);
};