#pragma once

#include <algorithm>
#include "Intercept.h"
#include "Curve.h"
#include "MeshRasterizer.h"

class Rasterizer2D: public MeshRasterizer {
public:
    explicit Rasterizer2D(vector<Intercept>& verts, bool cyclic = false)
            : verts(verts), cyclic(cyclic) {
        unsampleable = false;
    }

    void calcCrossPoints() override {
        if (verts.empty()) {
            cleanUp();
            return;
        }

        std::sort(verts.begin(), verts.end());

        if(cyclic) {
            padIcptsWrapped(verts, curves);
        } else {
            padIcpts(verts, curves);
        }

        float base = 0.1f / float(Curve::resolution);

        setResolutionIndices(base);

        for(auto & curve : curves) {
            curve.recalculateCurve();
        }

        calcWaveform();
        unsampleable = false;
    }

    void updateCurve(int index, const Intercept& position) {
        jassert(index < curves.size());

        if(index < 1 || index > (int) curves.size() - 2) {
            return;
        }

        Curve& prev = curves[index - 1];
        Curve& curve = curves[index + 0];
        Curve& next = curves[index + 1];

        prev.c.x = position.x;
        prev.c.y = position.y;

        prev.validate();
        prev.recalculateCurve();

        curve.b.x = position.x;
        curve.b.y = position.y;
        curve.b.shp = position.shp;

        curve.updateCurrentIndex();
        curve.validate();
        curve.recalculateCurve();

        next.a.x = position.x;
        next.a.y = position.y;
        next.validate();
        next.recalculateCurve();

        updateWaveform(index);
    }

    void updateWaveform(int index) {
        int res         = Curve::resolution / 2;
        int startIdx    = jmax(0, index - 1);
        int endIdx      = jmin((int) curves.size() - 1, index + 2);
        int waveIdx     = curves[startIdx].waveIdx;

        for (int c = startIdx; c < endIdx; ++c) {
            Curve& thisCurve    = curves[c];
            Curve& nextCurve    = curves[c + 1];

            jassert(waveIdx == curves[c].waveIdx);

            waveIdx             = curves[c].waveIdx;

            int indexA          = 0;
            int indexB          = 0;
            int minCurveRes     = jmin(res >> thisCurve.resIndex, res >> nextCurve.resIndex);
            int offset          = res >> thisCurve.resIndex;
            int xferInc         = Curve::resolution / minCurveRes;

            int thisShift       = jmax(0, (nextCurve.resIndex - thisCurve.resIndex));
            int nextShift       = jmax(0, (thisCurve.resIndex - nextCurve.resIndex));

            float xferValue;
            float t1x = 0, t1y = 0;
            float t2x = 0, t2y = 0;

            for (int i = 0; i < minCurveRes; ++i) {
                xferValue = transferTable[i * xferInc];
                indexA = (i << thisShift) + offset;
                indexB = (i << nextShift);

                t1x = thisCurve.transformX[indexA] * (1 - xferValue);
                t1y = thisCurve.transformY[indexA] * (1 - xferValue);

                t2x = nextCurve.transformX[indexB] * xferValue;
                t2y = nextCurve.transformY[indexB] * xferValue;

                waveX[waveIdx] = t1x + t2x;
                waveY[waveIdx] = t1y + t2y;

                ++waveIdx;
            }
        }

        int waveStart   = curves[startIdx].waveIdx;
        int waveEnd     = waveIdx;
        int size        = waveEnd - waveStart;

        if(waveEnd == waveX.size()) {
            --size;
        }

        Buffer<Float32> ex   = waveX.section(waveStart, size);
        Buffer<float> why   = waveY.section(waveStart, size);
        Buffer<float> diffx = diffX.section(waveStart, size);
        Buffer<float> slp   = slope.section(waveStart, size);

        ippsSub_32f(ex, ex + 1, diffx, size);
        ippsThreshold_LT_32f_I(diffx, size, 1e-06f);
        ippsSub_32f(why, why + 1, slp, size);
        ippsDiv_32f_I(diffx, slp, size);
    }

    void setCyclicity(bool isCyclic)    { cyclic = isCyclic;    }
    bool isCyclic() const               { return cyclic;        }
    static int getPaddingSize()         { return 2;             }

protected:
    bool cyclic;
    vector<Intercept>& verts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Rasterizer2D)
};
