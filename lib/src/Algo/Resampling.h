#pragma once

#include <vector>
#include "../Array/Buffer.h"
#include "../Curve/Vertex2.h"

using std::vector;

class Resampling {
public:

    enum { Linear, Hermite, BSpline, Sinc };

    Resampling();
    virtual ~Resampling() = default;

    // t is normalized between x3 and y3
    static float spline(float x0, float y0,
                        float x1, float y1,
                        float x2, float y2,
                        float x3, float y3, float t) {
        // normalize t
        t = (t - x2) / (x3 - x2);

        jassert(t >= 0 && t <= 1);

        float t2 = t * t;
        float t3 = t2 * t;

        float y = 0.5f * ((2.0f * y1) +
                    t * (-y0 + y2) +
                    t2 * (2.0f * y0 - 5.0f * y1 + 4 * y2 - y3) +
                    t3 * (-y0 + 3.0f * y1 - 3.0f * y2 + y3));

        return y;
    }

    static float at(float x, const vector<Vertex2>& points, int& currentIndex);

    static float hermite6_3(float x, float yn2, float yn1, float y0, float y1, float y2, float y3);
    static float bspline6_5(float x, float yn2, float yn1, float y0, float y1, float y2, float y3);

    static void linResample(const Buffer<float>& source, Buffer<float> dest) {
        float remainder, realPosition;
        int trunc;

        dest[0] = source[0];
        dest[dest.size() - 1] = source[source.size() - 1];

        float ratio = (source.size() - 1) / float(dest.size() - 1);

        for (int i = 1; i < dest.size() - 1; ++i) {
            realPosition    = i * ratio;
            trunc           = (int) realPosition;
            remainder       = realPosition - trunc;

            dest[i] = (1 - remainder) * source[trunc] + remainder * source[trunc + 1];
        }
    }

    static void linResample(const Buffer<float>& source, Buffer<float> dest, double sourceToDestRatio, double& phase);
    static void linResample2(const Buffer<float>& source, Buffer<float> dest,
                             double sourceToDestRatio, float& padA, float& padB, double& phase);

    static void resample(const Buffer<float>& source, Buffer<float> dest, double sourceToDestRatio,
                         float& p0, float& p1, float& p2, float& p3, float& p4, float& p5, float& p6,
                         double& phase, int algo);

    static float lerp(float a, float b, float pos);
    static float lerp(float x1, float y1, float x2, float y2, float pos);
    static float lerpC(Buffer<float> buff, float unitPos);
    static float lerp(const Vertex2& a, const Vertex2& b, float pos) {  return lerp(a.x, a.y, b.x, b.y, pos); }

    static float interpValueQuadratic(float y1, float y2, float y3);
    static float interpIndexQuadratic(float y1, float y2, float y3);

};