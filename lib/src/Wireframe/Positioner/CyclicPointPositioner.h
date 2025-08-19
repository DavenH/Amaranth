#pragma once

#include "PointPositioner.h"

/**
 * Enforces translational symmetry in graphic Oscillator-use curves.
 */
class CyclicPointPositioner : public PointPositioner {
public:
    void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config) override {
        size_t size = controlPoints.size();
        size_t end  = size - 1;

        if (end < 1) {
            return;
        }

        vector<Intercept> frontIcpts, backIcpts;

        float frontier     = 0.f;
        float offset       = -1.f;
        size_t idx         = end;
        int remainingIters = 2;

        frontIcpts.emplace_back(controlPoints[1]);
        frontIcpts.emplace_back(controlPoints[0]);

        for (;;) {
            if (remainingIters <= 0) {
                break;
            }

            if (frontier < -config.padding) {
                --remainingIters;
            }

            frontier          = controlPoints[idx].x + offset;
            Intercept padIcpt = controlPoints[idx];
            padIcpt.x += offset;

            frontIcpts.emplace_back(padIcpt);
            --idx;

            if (idx < 0) {
                idx = end;
                offset -= 1.f;
            }
        }

        remainingIters = 2;
        idx            = 0;
        frontier       = 1.f;
        offset         = 1;

        backIcpts.emplace_back(controlPoints[end - 1]);
        backIcpts.emplace_back(controlPoints[end]);

        for (;;) {
            if (remainingIters <= 0) {
                break;
            }

            if (frontier > 1.f + config.padding) {
                --remainingIters;
            }

            frontier          = controlPoints[idx].x + offset;
            Intercept padIcpt = controlPoints[idx];
            padIcpt.x += offset;

            backIcpts.emplace_back(padIcpt);
            ++idx;

            if (idx > end) {
                idx = 0;
                offset += 1.f;
            }
        }

        // Merge into controlPoints ensuring ascending x for the front padding
        std::reverse(frontIcpts.begin(), frontIcpts.end());
        controlPoints.insert(controlPoints.begin(), frontIcpts.begin(), frontIcpts.end());
        controlPoints.insert(controlPoints.end(), backIcpts.begin(), backIcpts.end());
    }

    // I think this should capture a lot more of the semantics of the operation.
    // We want to preserve lines that transit the 1.0 edge in a 2D waveshape.
    // We don't want one point being wrapped
    // We DO want both points wrapped if neither of them transit the 1.0 edge, i.e. both are > 1
    // now, since we're talking lines, I think we need to have it named wrapLines, or something.
    // Should there be a way to generate Line segments by a->line(zDim, Vertex::phase)?
    // Second thought, this doesn't seem to be a positioner thing necessarily. It's a line splitting policy.
    static void wrapTrilinearEdge(float& ax, float& ay, float& bx, float& by, float indie) {
        if (ay > 1 && by > 1) {
            ay -= 1;
            by -= 1;
        } else if (ay > 1 != by > 1) {
            float icpt = ax + (1 - ay) / ((ay - by) / (ax - bx));
            if (icpt > indie) {
                ay -= 1;
                by -= 1;
            }
        }
    }

    static bool wrapAndDidAnything(float& x) {
        float lastX = x;

        while (x >= 1.f) { x -= 1.f; }
        while (x < 0.f) { x += 1.f; }

        return lastX != x;
    }
};
