#pragma once

#include <climits>
#include <vector>

#include <App/AppConstants.h>
#include <Array/VecOps.h>

#include "../../WaveformBuffers.h"
#include "../../GuideCurveOffsetSeeds.h"
#include "../../../Curve.h"
#include "../../../GuideCurveProvider.h"
#include "../../../../Obj/MorphPosition.h"
#include "../../Sampling/GuideCurveSampler.h"

namespace Rasterization {
    class WaveformBakePolicy {
    public:
        struct Context {
            bool lowResCurves {};
            bool decoupleComponentDfrms {};
            int noiseSeed {};

            MorphPosition morph;
            GuideCurveProvider* guideCurveProvider {};
            std::vector<GuideCurveRegion>* guideCurveRegions {};
            const GuideCurveOffsetSeeds* offsetSeeds {};
            const float* transferTable {};

            WaveformBufferRefs waveform;
        };

        int prepare(std::vector<Curve>& curves, const Context& context) const {
            int res = Curve::resolution / 2;
            int totalRes = 0;
            int tableSize = Constants::GuideCurveTableSize;

            for (int i = 0; i < (int) curves.size() - 1; ++i) {
                Curve& thisCurve = curves[i];
                VertCube* cube = thisCurve.b.cube;

                int thisRes = res >> thisCurve.resIndex;
                int nextRes = res >> curves[i + 1].resIndex;

                if (hasComponentGuide(cube, context.guideCurveProvider)) {
                    int numVerts = context.guideCurveProvider->getTableDensity(cube->getCompGuideCurve());
                    int desiredRes = thisRes * (int) ((context.lowResCurves ? 2 : 8) * sqrtf(numVerts) + 0.49f);
                    float scaleRatio = tableSize / float(desiredRes);

                    if (curves.size() == 6) {
                        scaleRatio /= 2.f;
                    }

                    int truncRatio = jlimit(1, 256, int(scaleRatio + 0.5f));
                    thisCurve.curveRes = tableSize / truncRatio;
                } else {
                    thisCurve.curveRes = jmin(thisRes, nextRes);
                }

                totalRes += thisCurve.curveRes;
            }

            return totalRes;
        }

        void bake(std::vector<Curve>& curves, Context& context) const {
            int res = Curve::resolution / 2;
            int waveIdx = 0;
            int cumeRes = 0;

            *context.waveform.zeroIndex = 0;
            *context.waveform.oneIndex = INT_MAX / 2;

            for (int c = 0; c < (int) curves.size() - 1; ++c) {
                Curve& thisCurve = curves[c];
                Curve& nextCurve = curves[c + 1];
                int curveRes = thisCurve.curveRes;

                thisCurve.waveIdx = waveIdx;

                if (hasComponentGuide(thisCurve.b.cube, context.guideCurveProvider)) {
                    bakeGuideCurve(thisCurve, nextCurve, context, waveIdx, curveRes);
                } else {
                    bakeInterpolatedCurve(thisCurve, nextCurve, context, waveIdx, curveRes, res);
                }

                updateBoundaryIndices(context, cumeRes, curveRes);
                cumeRes += curveRes;
            }

            finalizeBuffers(context, cumeRes);
        }

    private:
        static bool hasComponentGuide(VertCube* cube, GuideCurveProvider* guideCurveProvider) {
            return cube != nullptr && guideCurveProvider != nullptr && cube->getCompGuideCurve() >= 0;
        }

        static void bakeGuideCurve(
                Curve& thisCurve,
                Curve& nextCurve,
                Context& context,
                int& waveIdx,
                int curveRes) {
            VertCube* cube = thisCurve.b.cube;
            int compDfrm = cube->getCompGuideCurve();

            Intercept& thisCentre = thisCurve.b;
            Intercept& nextCentre = nextCurve.b;

            GuideCurveProvider::NoiseContext noise;
            noise.noiseSeed   = context.noiseSeed < 0 ? context.morph.time.getCurrentValue() * (float) INT_MAX : context.noiseSeed;

            if (context.offsetSeeds != nullptr) {
                noise.phaseOffset = context.offsetSeeds->phaseAt(compDfrm);
                noise.vertOffset = context.offsetSeeds->verticalAt(compDfrm);
            }

            Buffer<float>& waveX = *context.waveform.waveX;
            Buffer<float>& waveY = *context.waveform.waveY;

            Buffer<Float32> yPortion(waveY + waveIdx, curveRes);
            Buffer<Float32> xPortion(waveX + waveIdx, curveRes);
            float multiplier = thisCentre.shp * cube->guideCurveAbsGain(Vertex::Time);

            if (context.decoupleComponentDfrms) {
                yPortion.zero();

                GuideCurveRegion region;
                region.amplitude   = multiplier;
                region.guideIndex  = cube->getCompGuideCurve();
                region.start       = thisCentre;
                region.end         = nextCentre;

                context.guideCurveRegions->emplace_back(region);
            } else {
                context.guideCurveProvider->sampleDownAddNoise(cube->getCompGuideCurve(), yPortion, noise);
                yPortion.mul(multiplier);
            }

            float invSize = 1.f / float(curveRes);
            float curveSlope = invSize * (nextCentre.y - thisCentre.y);
            Buffer<float> temp = xPortion;
            temp.ramp(thisCentre.y, curveSlope);
            yPortion.add(temp);

            float minX = jmin(thisCentre.x, nextCentre.x);
            float maxX = jmax(thisCentre.x, nextCentre.x);

            curveSlope = (maxX - minX) * invSize;
            xPortion.ramp(minX, curveSlope);

            waveIdx += curveRes;
        }

        static void bakeInterpolatedCurve(
                Curve& thisCurve,
                Curve& nextCurve,
                Context& context,
                int& waveIdx,
                int curveRes,
                int res) {
            int offset = res >> thisCurve.resIndex;
            int xferInc = Curve::resolution / curveRes;
            int thisShift = jmax(0, nextCurve.resIndex - thisCurve.resIndex);
            int nextShift = jmax(0, thisCurve.resIndex - nextCurve.resIndex);

            for (int i = 0; i < curveRes; ++i) {
                float xferValue = context.transferTable[i * xferInc];
                int indexA = (i << thisShift) + offset;
                int indexB = i << nextShift;

                float t1x = thisCurve.transformX[indexA] * (1.f - xferValue);
                float t1y = thisCurve.transformY[indexA] * (1.f - xferValue);
                float t2x = nextCurve.transformX[indexB] * xferValue;
                float t2y = nextCurve.transformY[indexB] * xferValue;

                (*context.waveform.waveX)[waveIdx] = t1x + t2x;
                (*context.waveform.waveY)[waveIdx] = t1y + t2y;

                ++waveIdx;
            }
        }

        static void updateBoundaryIndices(Context& context, int cumeRes, int curveRes) {
            Buffer<float>& waveX = *context.waveform.waveX;

            if (cumeRes > 0 && waveX[cumeRes - 1] <= 0.f && waveX[cumeRes] > 0.f) {
                *context.waveform.zeroIndex = cumeRes - 1;
            } else if (waveX[cumeRes] <= 0.f && waveX[cumeRes + curveRes - 1] > 0.f) {
                for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                    if (waveX[i] <= 0.f && waveX[i + 1] > 0.f) {
                        *context.waveform.zeroIndex = i;
                        break;
                    }
                }
            }

            if (cumeRes > 0 && waveX[cumeRes - 1] < 1.f && waveX[cumeRes] >= 1.f) {
                *context.waveform.oneIndex = cumeRes - 1;
            } else if (waveX[cumeRes] < 1.f && waveX[cumeRes + curveRes - 1] >= 1.f) {
                for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                    if (waveX[i] < 1.f && waveX[i + 1] >= 1.f) {
                        *context.waveform.oneIndex = i;
                        break;
                    }
                }
            }
        }

        static void finalizeBuffers(Context& context, int cumeRes) {
            Buffer<float>& waveX = *context.waveform.waveX;
            Buffer<float>& waveY = *context.waveform.waveY;
            Buffer<float>& diffX = *context.waveform.diffX;
            Buffer<float>& slope = *context.waveform.slope;
            Buffer<float>& area = *context.waveform.area;

            jassert(waveX.front() == waveX.front());

            if (*context.waveform.zeroIndex == 0 && waveX.front() < 0.f && waveX.back() > 0.f) {
                for (int i = 0; i < cumeRes - 1; ++i) {
                    if (waveX[i] <= 0.f && waveX[i + 1] > 0.f) {
                        *context.waveform.zeroIndex = i;
                    }
                }
            }

            jassert(cumeRes == waveX.size());

            int resSubOne = cumeRes - 1;

            Buffer<float> slp = slope.withSize(resSubOne);
            Buffer<float> dif = diffX.withSize(resSubOne);
            Buffer<float> are = area.withSize(resSubOne);

            VecOps::sub(waveX + 1, waveX, dif);
            VecOps::sub(waveY + 1, waveY, slp);
            VecOps::sub(waveY + 1, waveY, are);
            dif.threshLT(1e-6f);
            slp.div(dif);
            are.mul(dif).mul(0.5f);
        }
    };
}
