#pragma once

#include "PointPositioner.h"

class EnvPointPositioner : public PointPositioner {
public:
    void adjustControlPoints(vector<Intercept>& controlPoints, CurveParameters config) override {

        if (scalingType != Bipolar) {
            if (sustainIndex >= 0 && sustainIndex != intercepts.size() - 1) {
                Intercept sustIcpt = intercepts[sustainIndex];
                sustIcpt.cube = nullptr;
                sustIcpt.x += 0.0001f;

                if (sustIcpt.y < 0.5f) {
                    sustIcpt.y = 0.5f;
                }

                sustIcpt.shp = 1.f;

                intercepts.emplace_back(sustIcpt);

                needsResorting = true;
            }
        }
    }
};