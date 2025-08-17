#pragma once

#include <algorithm>
#include <vector>

#include "FXRasterizer.h"
#include "../Rasterizer/MeshRasterizer.h"
#include "../Interpolator/Simple/SimpleInterpolator.h"
#include "../Curve/SimpleCurveGenerator.h"
#include "../Positioner/PointPositioner.h"
#include "../Positioner/CyclicPointPositioner.h"
#include "../State/RasterizerParameters.h"
#include "../Sampler/SimpleCurveSampler.h"
#include <Array/ScopedAlloc.h>
#include <Array/VecOps.h>

class Rasterizer2D {
public:
    explicit Rasterizer2D(std::vector<Intercept>& verts, bool cyclic = false)
        : verts(verts) {
        if (cyclic) {
            this->positioner = new CyclicPointPositioner();
        }
    }

    void setInterceptPadding(float v)   { interceptPadding = v; }
    void setLimits(float min, float max){ xMinimum_ = min; xMaximum_ = max; }

    void generateControlPoints() override {
        if (verts.empty()) {
            cleanUp();
            return;
        }

        std::sort(verts.begin(), verts.end());

        if(cyclic) {
            padControlPointsWrapped(verts, curves);
        } else {
            padControlPoints(verts, curves);
        }

        float base = 0.1f / float(CurvePiece::resolution);

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

        CurvePiece& prev = curves[index - 1];
        CurvePiece& curve = curves[index + 0];
        CurvePiece& next = curves[index + 1];

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
        int res         = CurvePiece::resolution / 2;
        int startIdx    = jmax(0, index - 1);
        int endIdx      = jmin((int) curves.size() - 1, index + 2);
        int waveIdx     = curves[startIdx].waveIdx;

        for (int c = startIdx; c < endIdx; ++c) {
            CurvePiece& thisCurve    = curves[c];
            CurvePiece& nextCurve    = curves[c + 1];

            jassert(waveIdx == curves[c].waveIdx);

            waveIdx             = curves[c].waveIdx;

            int indexA          = 0;
            int indexB          = 0;
            int minCurveRes     = jmin(res >> thisCurve.resIndex, res >> nextCurve.resIndex);
            int offset          = res >> thisCurve.resIndex;
            int xferInc         = CurvePiece::resolution / minCurveRes;

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

        VecOps::diff(ex, diffx);
        diffx.threshLT(1e-6f);
        VecOps::diff(why, slp);
        slp.div(diffx);
        // ippsSub_32f(ex, ex + 1, diffx, size);
        // ippsThreshold_LT_32f_I(diffx, size, 1e-06f);
        // ippsSub_32f(why, why + 1, slp, size);
        // ippsDiv_32f_I(diffx, slp, size);
    }

    // Compatibility shim for existing callers
    void performUpdate(int /*updateType*/) { generateControlPoints(); }

    // Accessors used by tests and callers
    const std::vector<CurvePiece>& getCurves() const { return curves_; }
    Buffer<float> getWaveX() { return waveX_; }
    Buffer<float> getWaveY() { return waveY_; }
    bool wasCleanedUp() const { return unsampleable_; }

private:
    static int getPaddingSize() { return 2; }

    // TODO doesn't belong here -- belongs in CurveSampler
    void updateBuffers(int size) {
        const int numBuffers = 4;
        memory_.ensureSize(size * numBuffers);
        waveX_ = memory_.place(size);
        waveY_ = memory_.place(size);
        diffX_ = memory_.place(size);
        slope_ = memory_.place(size);
    }

    // TODO doesn't belong here -- belongs in CurveGenerator
    void setResolutionIndices(float base) {
        if (curves_.empty()) return;
        int lastIdx = (int)curves_.size() - 1;
        for (int i = 1; i < (int)curves_.size() - 1; ++i) {
            float dx = (curves_[i + 1].c.x - curves_[i - 1].a.x);
            for (int j = 0; j < CurvePiece::resolutions; ++j) {
                int res = CurvePiece::resolution >> j;
                if (dx < base * float(res)) {
                    curves_[i].resIndex = j;
                };
            }
        }
        int padding = getPaddingSize();
        curves_.front().resIndex = curves_[lastIdx - 2 * (padding - 1)].resIndex;
        curves_.back().resIndex  = curves_[2 * padding - 1].resIndex;
    }

    // State
    std::vector<Intercept>& verts;
    bool cyclic{};
    float interceptPadding{};
    float xMinimum_{0.f}, xMaximum_{1.f};
    bool unsampleable_{false};
    int zeroIndex_{0}, oneIndex_{0};

    std::vector<CurvePiece> curves_;
    ScopedAlloc<float> memory_;
    Buffer<float> waveX_, waveY_, diffX_, slope_;
};
