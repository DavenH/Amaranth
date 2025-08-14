#pragma once
#include "../Vertex/Intercept.h"
#include "Wireframe/State/CurveParameters.h"

class PointPositioner {
public:
    virtual ~PointPositioner() = default;
    virtual void adjustControlPoints(vector<Intercept>& controlPoints, CurveParameters config) {
        int end = controlPoints.size() - 1;

        if (end == 0) {
            return;
        }

        float maxX = -1, minX = 1;
        for (auto& icpt : controlPoints) {
            float x = icpt.x;
            maxX = x > maxX ? x : maxX;
            minX = x < minX ? x : minX;
        }

        minX = jmin(config.minX - 0.25f, minX);
        maxX = jmax(config.maxX + 0.25f, maxX);

        Intercept front1(minX - 0.24f, controlPoints[0].y);
        Intercept front2(minX - 0.16f, controlPoints[0].y);
        Intercept front3(minX - 0.08f, controlPoints[0].y);

        Intercept back1(maxX + 0.08f,  controlPoints[end].y);
        Intercept back2(maxX + 0.16f,  controlPoints[end].y);
        Intercept back3(maxX + 0.24f,  controlPoints[end].y);

        controlPoints.insert(controlPoints.begin(), {front1, front2, front3});
        controlPoints.insert(controlPoints.end(), {back1, back2, front3});
    };
};
