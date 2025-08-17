#pragma once

#include "PointPositioner.h"

/**
 * Enforces translational symmetry in graphic Oscillator-use curves.
 */
class CyclicPointPositioner : public PointPositioner {
public:
    void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config) override {
        int size = controlPoints.size();
        int end  = size - 1;

        if (end < 1) {
            return;
        }

        vector<Intercept> frontIcpts, backIcpts;

        float frontier     = 0.f;
        float offset       = -1.f;
        int idx            = end;
        int remainingIters = 2;

        frontIcpts.emplace_back(controlPoints[1]);
        frontIcpts.emplace_back(controlPoints[0]);

        for (;;) {
            if (remainingIters <= 0) {
                break;
            }

            if (frontier < -interceptPadding) {
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

            if (frontier > 1.f + interceptPadding) {
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

    void adjustSingleControlPoint()
    void setInterceptPadding(float value) { interceptPadding = value; }

private:
    float interceptPadding {};
};
