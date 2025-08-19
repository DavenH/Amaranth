#pragma once

#include "../Vertex/Intercept.h"
#include "../Rasterizer/RasterizerParams.h"
#include <Util/Util.h>

using std::vector;

class PaddingPointPositioner {
public:
    explicit PaddingPointPositioner(const vector<float>& padding) : padding(padding) {}
    virtual ~PaddingPointPositioner() = default;

    virtual void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config)  {
        int end = controlPoints.size() - 1;

        if (end <= 0) {
            return;
        }

        float maxX = config.minX, minX = config.maxX;
        if (config.useMinMax) {
            getMinMax(controlPoints, minX, maxX);
        }

        for (auto pad : this->padding) {
            Intercept icpt(minX - pad, controlPoints[0].y);
            controlPoints.insert(controlPoints.begin(), icpt);
        }

        for (auto pad : Util::reverseIter(this->padding)) {
            Intercept icpt(maxX + pad, controlPoints[end].y);
            controlPoints.insert(controlPoints.end(), icpt);
        }
    }

    virtual void getMinMax(vector<Intercept>& controlPoints, float& minX, float& maxX) {
        for (auto& icpt : controlPoints) {
            float x = icpt.x;
            maxX = x > maxX ? x : maxX;
            minX = x < minX ? x : minX;
        }
    }

private:
    vector<float> padding;
};
