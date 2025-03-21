#include <iterator>
#include <cmath>
#include <climits>
#include "IDeformer.h"
#include "Mesh.h"
#include "MeshRasterizer.h"

#include "VertCube.h"
#include "../App/AppConstants.h"
#include "../App/MeshLibrary.h"
#include "../Array/ScopedAlloc.h"
#include "../Design/Updating/Updater.h"
#include "../Util/CommonEnums.h"
#include "../Util/NumberUtils.h"
#include "../Util/Util.h"

float MeshRasterizer::transferTable[Curve::resolution];

MeshRasterizer::MeshRasterizer(const String& name) :
         name                (name)
    ,    mesh                (nullptr)
    ,    deformer            (nullptr)

    ,    zeroIndex           (0)
    ,    paddingSize         (2)
    ,    oneIndex            (INT_MAX / 2)
    ,    noiseSeed           (-1)
    ,    overridingDim       (Vertex::Time)

    ,    interceptPadding    (0.f)
    ,    xMinimum            (0.f)
    ,    xMaximum            (1.f)

    ,    calcDepthDims       (true)
    ,    decoupleComponentDfrms(false)
    ,    calcInterceptsOnly  (false)
    ,    integralSampling    (false)
    ,    lowResCurves        (false)
    ,    overrideDim         (false)
    ,    unsampleable        (true)
    ,    cyclic              (true)
    ,    scalingType         (Unipolar)
    ,    needsResorting      (false)
    ,    interpolateCurves   (false)
    ,    batchMode           (false) {
    initialise();
}


MeshRasterizer::~MeshRasterizer() {
    waveX.nullify();
    waveY.nullify();
    diffX.nullify();
    slope.nullify();
    area.nullify();

    memoryBuffer.clear();

    curves.clear();
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();

    makeCopy();
}

void MeshRasterizer::calcCrossPointsAtTime(float x) {
    morph.time = x;
    calcCrossPoints();
}

void MeshRasterizer::calcCrossPoints() {
    calcCrossPoints(mesh, 0.f);
}

void MeshRasterizer::calcWaveformFrom(vector<Intercept>& icpts) {
    this->icpts = icpts;

    padIcpts(icpts, curves);
    updateCurves();
    makeCopy();
}

void MeshRasterizer::calcCrossPoints(Mesh* usedMesh, float oscPhase) {
    if (!usedMesh || usedMesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    icpts.clear();
    preCleanup();

    int zDim = overrideDim ? overridingDim : getPrimaryViewDimension();

    float independent = zDim == Vertex::Time ? morph.time :
                        zDim == Vertex::Red ?  morph.red  : morph.blue;

    float poleA[3];
    float poleB[3];
    int indDims[3] = { Vertex::Time, Vertex::Red, Vertex::Blue };

    Vertex middle;
    Intercept midIcpt;

    // must be set before applyGuides call
    needsResorting = false;

    if (calcDepthDims) {
        colorPoints.clear();
    }

    for (auto cube : usedMesh->getCubes()) {
        cube->getInterceptsFast(zDim, reduct, morph);

        if (reduct.pointOverlaps) {
            Vertex* a = &reduct.v0;
            Vertex* b = &reduct.v1;
            Vertex* vertex = &reduct.v;

            if (calcDepthDims) {
                VertCube::vertexAt(independent, zDim, a, b, vertex);

                midIcpt.x = vertex->values[Vertex::Phase] + oscPhase;
                midIcpt.y = vertex->values[Vertex::Amp];
            }

            wrapVertices(a->values[zDim], a->values[Vertex::Phase],
                         b->values[zDim], b->values[Vertex::Phase],
                         independent);

            VertCube::vertexAt(independent, zDim, a, b, vertex);

            float x = vertex->values[Vertex::Phase] + oscPhase;

            if (cyclic) {
                while (x >= 1.f) x -= 1.f;
                while (x < 0.f) x += 1.f;

                jassert(x >= 0.f && x < 1.f);
                jassert(xMaximum == 1.f && xMinimum == 0.f);
            } else {
                NumberUtils::constrain(x, xMinimum, xMaximum);
            }

            Intercept intercept(x, vertex->values[Vertex::Amp], cube);

            intercept.shp = vertex->values[Vertex::Curve];
            intercept.adjustedX = intercept.x;

            jassert(intercept.y == intercept.y);
            jassert(intercept.x == intercept.x);

            // can be NaN, short circuit here so it doesn't propagate
            if(!(intercept.y == intercept.y))
                intercept.y = 0.5f;

            switch (scalingType) {
                case Unipolar:
                    break;

                case Bipolar:
                    intercept.y = 2 * intercept.y - 1;
                    break;

                case HalfBipolar:
                    intercept.y -= 0.5f;
                    break;

                default: break;
            }

            applyDeformers(intercept, morph);
            icpts.emplace_back(intercept);

            int currentlyVisibleRYBDim = getPrimaryViewDimension();
            if (calcDepthDims) {
                midIcpt.cube = cube;
                midIcpt.adjustedX = midIcpt.x;
                applyDeformers(midIcpt, morph);

                for (int i = 0; i < dims.numHidden(); ++i) {
                    Vertex2 midCopy(midIcpt.adjustedX, midIcpt.y);

                    int hiddenDim = dims.hidden[i];

                    for (int j = 0; j < numElementsInArray(indDims); ++j) {
                        poleA[j] = hiddenDim == indDims[j] ? 0.f : morph[indDims[j]];
                        poleB[j] = hiddenDim == indDims[j] ? 1.f : morph[indDims[j]];
                    }

                    MorphPosition posA(poleA[0], poleA[1], poleA[2]);
                    cube->getFinalIntercept(reduct, posA);
                    Intercept beforeIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
                    beforeIcpt.adjustedX = beforeIcpt.x;
                    applyDeformers(beforeIcpt, posA, hiddenDim == currentlyVisibleRYBDim);

                    MorphPosition posB(poleB[0], poleB[1], poleB[2]);
                    cube->getFinalIntercept(reduct, posB);
                    Intercept afterIcpt(reduct.v.values[dims.x], reduct.v.values[dims.y], cube);
                    afterIcpt.adjustedX = afterIcpt.x;
                    applyDeformers(afterIcpt, posB, hiddenDim == currentlyVisibleRYBDim);

                    Vertex2 before(beforeIcpt.adjustedX, beforeIcpt.y);
                    Vertex2 after(afterIcpt.adjustedX, afterIcpt.y);

                    if (cyclic) {
                        if ((before.x > 1) != (after.x > 1)) {
                            Vertex2 before2 = before;
                            Vertex2 after2    = after;
                            Vertex2 mid2     = midCopy;

                            after2.x  -= 1.f;
                            before2.x -= 1.f;
                            mid2.x       -= 1.f;

                            colorPoints.emplace_back(cube, before2, mid2, after2, hiddenDim);
                        }

                        if (before.x > 1 && after.x > 1) {
                            after.x   -= 1.f;
                            before.x  -= 1.f;
                            midCopy.x -= 1.f;
                        }
                    }

                    colorPoints.emplace_back(cube, before, midCopy, after, hiddenDim);
                }
            }
        }
    }

    // sorts by x-value, not adjusted x,
    std::sort(icpts.begin(), icpts.end());

    // and set x = adjustedX
    restrictIntercepts(icpts);

    processIntercepts(icpts);

    if(Util::assignAndWereDifferent(needsResorting, false))
        std::sort(icpts.begin(), icpts.end());

    int end = icpts.size() - 1;
    if (end < 0) {
        cleanUp();

        return;
    }

    if (end == 0) {
        curves.clear();

        waveX.nullify();
        waveY.nullify();

        unsampleable = true;
    }

    bool padAny = false;

    for (int i = 0; i < (int) icpts.size() - 1; ++i) {
        Intercept& curr = icpts[i];
        Intercept& next = icpts[i + 1];
        bool pad        = curr.cube != nullptr && curr.cube->getCompDfrm() >= 0;
        curr.padBefore     = pad;
        next.padAfter     = pad;

        padAny |= pad;
    }

    if (padAny) {
        for(int i = 1; i < icpts.size(); ++i)
            icpts[i].padBefore &= ! icpts[i - 1].padBefore;

        for(int i = 0; i < (int) icpts.size() - 1; ++i)
            icpts[i].padAfter &= ! icpts[i + 1].padAfter;
    }

    if (!calcInterceptsOnly) {
        curves.clear();

        if (cyclic) {
            padIcptsWrapped(icpts, curves);
        } else {
            padIcpts(icpts, curves);
        }

        updateCurves();
    }
}

int MeshRasterizer::getPrimaryViewDimension() {
    return Vertex::Time;
}

void MeshRasterizer::calcIntercepts() {
    ScopedValueSetter calcIcptsFlag(calcInterceptsOnly, true, calcInterceptsOnly);

    calcCrossPoints();
    makeCopy();
}

void MeshRasterizer::calcTransferTable() {
    static bool alreadyCalculated = false;
    if(alreadyCalculated) {
        return;
    }

    const float pi = MathConstants<float>::pi;

    double isize = 1 / double(Curve::resolution);
    double x;
    for (int i = 0; i < Curve::resolution; ++i) {
        x = i * isize;
        transferTable[i] = x -
                0.2180285f * sinf(2.f * pi * x) +
                0.0322599f * sinf(4.f * pi * x) -
                0.0018794f * sinf(6.f * pi * x);
    }

    alreadyCalculated = true;
}

void MeshRasterizer::restrictIntercepts(vector<Intercept>& intercepts) {
    if (intercepts.empty()) {
        return;
    }

    for (auto & intercept : intercepts) {
        NumberUtils::constrain(intercept.adjustedX, xMinimum, xMaximum);
    }

    // ensure strict sort order
    for (int i = 1; i < (int) intercepts.size(); ++i) {
        Intercept& a = intercepts[i - 1];
        Intercept& b = intercepts[i];

        if (b.adjustedX < a.adjustedX) {
            if(b.isWrapped == a.isWrapped) {
                b.adjustedX = a.adjustedX + 0.0001f;
            }
        }
    }

    for (auto & intercept : intercepts) {
        intercept.x = intercept.adjustedX;
    }

    if (intercepts.back().x >= xMaximum) {
        for (int i = intercepts.size() - 1; i >= 1; --i) {
            Intercept& left = intercepts[i - 1];
            Intercept& right = intercepts[i];

            if(left.x >= right.x)
                left.x = right.x - 0.0001f;
        }
    }

//    if(intercepts.front().x <= 0)
    {
        for (int i = 1; i < (int) intercepts.size(); ++i) {
            Intercept& left = intercepts[i - 1];
            Intercept& right = intercepts[i];

            if(right.x <= left.x)
                right.x = left.x + 0.0001f;
        }
    }

  #ifdef _DEBUG
    for(int i = 0; i < intercepts.size() - 1; ++i)
    {
        jassert(intercepts[i].x < intercepts[i + 1].x);
    }
  #endif
}

/*
 * set curve after a amp-vs-phase deformer to full sharpness
 * so that waveX is continuous. At < 1 sharpness, the trailing
 * x-values of the curve create a discontinuity
 */
void MeshRasterizer::adjustDeformingSharpness() {
    for (int i = 0; i < (int) curves.size(); ++i) {
        Curve& curve = curves[i];

        if (i < (int) curves.size() - 1 && curve.b.cube != nullptr) {
            if (curve.b.cube->getCompDfrm() >= 0) {
                Curve& next = curves[i + 1];

                if (next.b.cube == nullptr || next.b.cube->getCompDfrm() < 0) {
                    curve.c.shp = 1;
                    next.b.shp = 1;
                    next.updateCurrentIndex();

                    if (i < (int) curves.size() - 2) {
                        curves[i + 2].a.shp = 1;
                    }
                }
            }
        }
    }
}

void MeshRasterizer::updateCurves() {
    if (icpts.size() < 2) {
        return;
    }

    if (lowResCurves && curves.size() > 8) {
        for (auto & curve : curves) {
            curve.resIndex = Curve::resolutions - 1;
            curve.setShouldInterpolate(false);
        }
    } else {
        for(auto & curve : curves)
            curve.setShouldInterpolate(! lowResCurves && interpolateCurves);

        float baseFactor = lowResCurves ? 0.4f : integralSampling ? 0.05f : 0.1f;
        float base          = baseFactor / float(Curve::resolution);

        setResolutionIndices(base);
    }

    adjustDeformingSharpness();

    for (auto & curve : curves) {
        curve.recalculateCurve();
    }

    calcWaveform();
}

void MeshRasterizer::calcWaveform() {
    int res = Curve::resolution / 2;
    int totalRes = 0;

    if(decoupleComponentDfrms)
        deformRegions.clear();

    int tableSize = Constants::DeformTableSize;

    for (int i = 0; i < (int) curves.size() - 1; ++i) {
        Curve& thisCurve = curves[i];
        VertCube* cube     = thisCurve.b.cube;

        int thisRes     = res >> thisCurve.resIndex;
        int nextRes     = res >> curves[i + 1].resIndex;

        if(cube != nullptr && deformer != nullptr && cube->getCompDfrm() >= 0) {
            int numVerts         = deformer->getTableDensity(cube->getCompDfrm());
            int desiredRes         = thisRes * (int) ((lowResCurves ? 2 : 8) * sqrtf(numVerts) + 0.49f);
            float scaleRatio     = tableSize / float(desiredRes);

            if(curves.size() == 6)
                scaleRatio /= 2;

            int truncRatio         = jlimit(1, 256, int(scaleRatio + 0.5));
            thisCurve.curveRes     = tableSize / truncRatio;
        } else {
            // take the minimum of adjacent curves
            thisCurve.curveRes = jmin(thisRes, nextRes);
        }

        totalRes += thisCurve.curveRes;
    }

    updateBuffers(totalRes);

    zeroIndex     = 0;
    oneIndex     = INT_MAX / 2;
    int waveIdx = 0;
    int cumeRes = 0;

    for (int c = 0; c < (int) curves.size() - 1; ++c) {
        Curve& thisCurve = curves[c];
        Curve& nextCurve = curves[c + 1];

        VertCube* cube = thisCurve.b.cube;

        int indexA = 0, indexB = 0;
        int curveRes = thisCurve.curveRes;
        int offset   = res >> thisCurve.resIndex;
        int xferInc  = Curve::resolution / curveRes;

        int thisShift = jmax(0, (nextCurve.resIndex - thisCurve.resIndex));
        int nextShift = jmax(0, (thisCurve.resIndex - nextCurve.resIndex));

        thisCurve.waveIdx = waveIdx;

        if (cube != nullptr && deformer != nullptr && cube->getCompDfrm() >= 0) {
            int compDfrm = cube->getCompDfrm();

            Intercept& thisCentre = thisCurve.b;
            Intercept& nextCentre = nextCurve.b;

            IDeformer::NoiseContext noise;
            noise.noiseSeed   = noiseSeed < 0 ? morph.time.getCurrentValue() * INT_MAX : noiseSeed;
            noise.phaseOffset = phaseOffsetSeeds[compDfrm];
            noise.vertOffset  = vertOffsetSeeds[compDfrm];

            Buffer<Float32> yPortion(waveY + waveIdx, curveRes);
            Buffer<Float32> xPortion(waveX + waveIdx, curveRes);

            float multiplier = thisCentre.shp * cube->deformerAbsGain(Vertex::Time);

            if (decoupleComponentDfrms) {
                yPortion.zero();

                DeformRegion region;
                region.amplitude   = multiplier;
                region.deformChan  = cube->getCompDfrm();
                region.start       = thisCentre;
                region.end         = nextCentre;

                deformRegions.emplace_back(region);
            } else {
                deformer->sampleDownAddNoise(cube->getCompDfrm(), yPortion, noise);
                yPortion.mul(multiplier);
            }

            float invSize = 1 / float(curveRes);
            float curveSlope = invSize * (nextCentre.y - thisCentre.y);

            // NB reusing the X array, no meaning to its name here
            Buffer<float> temp = xPortion;
            temp.ramp(thisCentre.y, curveSlope);
            yPortion.add(temp);

            float minX = jmin(thisCentre.x, nextCentre.x);
            float maxX = jmax(thisCentre.x, nextCentre.x);

            curveSlope = (maxX - minX) * invSize;
            xPortion.ramp(minX, curveSlope);

            waveIdx += curveRes;
        } else {
            float xferValue;
            float t1x = 0, t1y = 0;
            float t2x = 0, t2y = 0;

            for (int i = 0; i < curveRes; ++i) {
                xferValue   = transferTable[i * xferInc];
                indexA      = (i << thisShift) + offset;
                indexB      = (i << nextShift);

                t1x         = thisCurve.transformX[indexA] * (1 - xferValue);
                t1y         = thisCurve.transformY[indexA] * (1 - xferValue);
                t2x         = nextCurve.transformX[indexB] * xferValue;
                t2y         = nextCurve.transformY[indexB] * xferValue;

                waveX[waveIdx] = t1x + t2x;
                waveY[waveIdx] = t1y + t2y;

                ++waveIdx;
            }
        }

        if(cumeRes > 0 && waveX[cumeRes - 1] <= 0 && waveX[cumeRes] > 0) {
            zeroIndex = cumeRes - 1;
        }

        else if (waveX[cumeRes] <= 0 && waveX[cumeRes + curveRes - 1] > 0) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] <= 0 && waveX[i + 1] > 0) {
                    zeroIndex = i;
                    break;
                }
            }
        }

        if (cumeRes > 0 && waveX[cumeRes - 1] < 1 && waveX[cumeRes] >= 1) {
            oneIndex = cumeRes - 1;
        }

        else if (waveX[cumeRes] < 1 && waveX[cumeRes + curveRes - 1] >= 1) {
            for (int i = cumeRes; i < cumeRes + curveRes - 1; ++i) {
                if (waveX[i] < 1 && waveX[i + 1] >= 1) {
                    oneIndex = i;
                    break;
                }
            }
        }

        cumeRes += curveRes;
    }

    jassert(waveX.front() == waveX.front());

    if (zeroIndex == 0) {
        // this means that something screwed up with the curves
//        jassertfalse;

        if (waveX.front() < 0 && waveX.back() > 0) {
            for (int i = 0; i < cumeRes - 1; ++i) {
                if(waveX[i] <= 0 && waveX[i + 1] > 0)
                    zeroIndex = i;
            }
        }
    }

    jassert(cumeRes == waveX.size());

    int resSubOne = cumeRes - 1;

    Buffer<float> slp = slope.withSize(resSubOne);
    Buffer<float> dif = diffX.withSize(resSubOne);
    Buffer<float> are = area.withSize(resSubOne);

    VecOps::sub(waveX, waveX + 1, dif);
    VecOps::sub(waveY, waveY + 1, slp);
    VecOps::sub(waveY, waveY + 1, are);
    dif.threshLT(1e-6f);
    slp.div(dif);
    are.mul(dif).mul(0.5f);

    unsampleable = false;
}

float MeshRasterizer::sampleAt(double angle) {
    int currentIndex = Arithmetic::binarySearch(angle, waveX);
    return sampleAt(angle, currentIndex);
}

/*
 * For single sample per cycle rasterization, e.g. envelopes
 */
float MeshRasterizer::sampleAtDecoupled(double angle, DeformContext& context) {
    float val = sampleAt(angle, context.currentIndex);

    for(auto& region : deformRegions) {
        if (NumberUtils::within<float>(angle, region.start.x, region.end.x)) {
            float diff = region.end.x - region.start.x;

            if (diff > 0) {
                IDeformer::NoiseContext noise;
                noise.noiseSeed     = noiseSeed;
                noise.phaseOffset     = context.phaseOffsetSeed;
                noise.vertOffset     = context.vertOffsetSeed;

                float progress = (angle - region.start.x) / diff;

                return val + region.amplitude * deformer->getTableValue(region.deformChan, progress, noise);
            }
        }
    }

    return val;
}

// make damn sure that the last element in waveX is greater than 1 before calling this
float MeshRasterizer::sampleAt(double angle, int& currentIndex) {
    if (unsampleable || (float) angle >= waveX.back() || (float) angle < waveX.front()) {
        jassertfalse;
        return angle;
    }

    if (currentIndex >= (int) waveX.size()) {
        currentIndex = 0;
    }

    if (currentIndex > 0) {
        while (angle < waveX[currentIndex - 1]) {
            --currentIndex;
            if(currentIndex == 0)
                currentIndex = waveX.size() - 1;
        }
    }

    while (angle >= waveX[currentIndex]) {
        ++currentIndex;

        if(currentIndex >= (int) waveX.size()) {
            currentIndex = 0;
        }
    }

    if(currentIndex == 0) {
        return waveY[0];
    }

    return (angle - waveX[currentIndex - 1]) * slope[currentIndex - 1] + waveY[currentIndex - 1];
}


void MeshRasterizer::sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) {
    jassert(intervals.size() >= dest.size());

    if (waveX.empty()) {
        dest.set(0.5f);

        return;
    }

    float* destPtr = dest.get();
    const float* intervalsPtr = intervals.get();

    jassert(waveX.front() < intervals.front() && waveX.back() > intervals[dest.size() - 1]);

    int currentIndex = zeroIndex;

    for (int i = 0; i < (int) dest.size(); ++i) {
        while (intervalsPtr[i] >= waveX[currentIndex]) {
            currentIndex++;
        }

        destPtr[i] = (intervalsPtr[i] - waveX[currentIndex - 1]) * slope[currentIndex - 1] + waveY[currentIndex - 1];
    }
}


float MeshRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    float* dest = buffer.get();
    int size     = buffer.size();

    double areaScale = 1. / delta;
    double halfDiff = 0.5 * delta;
    double accum = phase - halfDiff;

    jassert(waveX.front() <= accum);
    jassert(accum + (size + 0.5) * delta <= waveX.back());

    int currentIndex = zeroIndex;

    while(accum < waveX[currentIndex] && currentIndex > 0) {
        --currentIndex;
    }
    while(accum >= waveX[currentIndex + 1] && currentIndex < waveX.size()) {
        ++currentIndex;
    }

    jassert(waveX[currentIndex] <= accum && waveX[currentIndex + 1] >= accum);

    if(accum < waveX.front()) {
        accum = waveX.front();
        currentIndex = 0;
    }

    int prevIdx, diffIdx;
    float x, diffX;

    for (int i = 0; i < size; ++i) {
        prevIdx = currentIndex;
        accum += delta;

        while(accum >= waveX[currentIndex]) {
            ++currentIndex;
        }

        --currentIndex;
        diffIdx = currentIndex - prevIdx;

        if(diffIdx == 0) {
            dest[i] = ((accum - halfDiff - waveX[currentIndex]) * slope[currentIndex] + waveY[currentIndex]);
        } else {
            x = accum - delta;
            dest[i] = 0.5f * (slope[prevIdx] * (x - waveX[prevIdx]) + waveY[prevIdx] + waveY[prevIdx + 1]) * (waveX[prevIdx + 1] - x);

            for(int j = 0; j < diffIdx - 1; ++j) {
                dest[i] += area[prevIdx + j + 1];
            }

            diffX = accum - waveX[currentIndex];
            dest[i] += (0.5f * slope[currentIndex] * diffX + waveY[currentIndex]) * diffX;

            dest[i] *= areaScale;
        }
    }

    phase = accum + halfDiff;

    if(phase > 0.5) {
        phase -= 1;
    }

    return phase;
}

void MeshRasterizer::reset() {
    curves.clear();
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();

    waveX.nullify();
    waveY.nullify();
    colorPoints.clear();

    unsampleable = true;
}

void MeshRasterizer::padIcptsWrapped(vector<Intercept>& intercepts, vector<Curve>& curves) {
    int size = intercepts.size();
    int end = size - 1;

    if(end < 1) {
        return;
    }

    frontIcpts.clear();
    backIcpts.clear();

    float frontier         = 0.f;
    float offset         = -1.f;
    int idx             = end;
    int remainingIters     = 2;

    frontIcpts.emplace_back(intercepts[1]);
    frontIcpts.emplace_back(intercepts[0]);

    for (;;) {
        if(remainingIters <= 0) {
            break;
        }

        if(frontier < -interceptPadding) {
            --remainingIters;
        }

        frontier             = intercepts[idx].x + offset;
        Intercept padIcpt     = intercepts[idx];
        padIcpt.x             += offset;

        frontIcpts.emplace_back(padIcpt);
        --idx;

        if (idx < 0) {
            idx = end;
            offset             -= 1.f;
        }
    }

    remainingIters             = 2;
    idx                     = 0;
    frontier                 = 1.f;
    offset                     = 1;

    backIcpts.emplace_back(intercepts[end - 1]);
    backIcpts.emplace_back(intercepts[end]);

    for(;;) {
        if(remainingIters <= 0) {
            break;
        }

        if(frontier > 1.f + interceptPadding) {
            --remainingIters;
        }

        frontier             = intercepts[idx].x + offset;
        Intercept padIcpt     = intercepts[idx];
        padIcpt.x             += offset;

        backIcpts.emplace_back(padIcpt);
        ++idx;

        if (idx > end) {
            idx = 0;
            offset += 1.f;
        }
    }

    paddingSize = frontIcpts.size() - 3;

    curves.clear();
    curves.reserve(intercepts.size() + frontIcpts.size() - 2 + backIcpts.size() - 2);

    for (int i = 0; i < (int) frontIcpts.size() - 2; ++i) {
        int idx = frontIcpts.size() - 1 - i;
        curves.emplace_back(frontIcpts[idx], frontIcpts[idx - 1], frontIcpts[idx - 2]);
    }

    for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
        curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
    }

    for (int i = 0; i < (int) backIcpts.size() - 2; ++i) {
        curves.emplace_back(backIcpts[i], backIcpts[i + 1], backIcpts[i + 2]);
    }
}

void MeshRasterizer::padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves) {
    int end = intercepts.size() - 1;

    if (end == 0) {
        waveX.nullify();
        waveY.nullify();
        unsampleable = true;

        return;
    }

    float maxX = -1, minX = 1;
    for (auto& icpt : icpts) {
        float x = icpt.x;
        maxX = x > maxX ? x : maxX;
        minX = x < minX ? x : minX;
    }

    minX = jmin(xMinimum - 0.25f, minX);
    maxX = jmax(xMaximum + 0.25f, maxX);

    paddingSize = 2;

    Intercept front1(minX - 0.08f,     intercepts[0].y);
    Intercept front2(minX - 0.16f,     intercepts[0].y);
    Intercept front3(minX - 0.24f,     intercepts[0].y);

    Intercept back1(maxX + 0.08f,     intercepts[end].y);
    Intercept back2(maxX + 0.16f,     intercepts[end].y);
    Intercept back3(maxX + 0.24f,     intercepts[end].y);

    curves.clear();
    curves.reserve(intercepts.size() + 6);
    curves.emplace_back(front3, front2, front1);
    curves.emplace_back(front2, front1, intercepts[0]);
    curves.emplace_back(front1, intercepts[0], intercepts[1]);

    jassert(intercepts[0].y == intercepts[0].y);
    for(int i = 0; i < (int) intercepts.size() - 2; ++i) {
        curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
    }

    curves.emplace_back(intercepts[end - 1], intercepts[end], back1);
    curves.emplace_back(intercepts[end], back1, back2);
    curves.emplace_back(back1, back2, back3);
}

void MeshRasterizer::validateCurves() {
    for (int i = 0; i < (int) curves.size() - 1; ++i) {
        jassert(curves[i].b.x == curves[i + 1].a.x);
        jassert(curves[i].c.x == curves[i + 1].b.x);
    }
}

// NB: set the intercept's adjustedX property rather than x
// it will be used to re-sort and then assign the x property
void MeshRasterizer::applyDeformers(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds) {
    if(icpt.cube == nullptr || deformer == nullptr) {
        return;
    }

    VertCube* cube = icpt.cube;

    IDeformer::NoiseContext noise;
    noise.noiseSeed = noiseSeed;

    if (cube->deformerAt(Vertex::Red) >= 0) {
        int dfrm = cube->deformerAt(Vertex::Red);
        float progress = cube->getPortionAlong(Vertex::Red, morph);

        noise.vertOffset  = vertOffsetSeeds [dfrm];
        noise.phaseOffset = phaseOffsetSeeds[dfrm];

        icpt.adjustedX += cube->deformerAbsGain(Vertex::Red) * deformer->getTableValue(dfrm, progress, noise);
    }

    if (cube->deformerAt(Vertex::Blue) >= 0) {
        int dfrm = cube->deformerAt(Vertex::Blue);
        float progress = cube->getPortionAlong(Vertex::Blue, morph);

        noise.vertOffset  = vertOffsetSeeds [dfrm];
        noise.phaseOffset = phaseOffsetSeeds[dfrm];

        icpt.adjustedX += cube->deformerAbsGain(Vertex::Blue) * deformer->getTableValue(dfrm, progress, noise);
    }

    float timeMin = reduct.v0.values[Vertex::Time];
    float timeMax = reduct.v1.values[Vertex::Time];

    if(timeMin > timeMax) {
        std::swap(timeMin, timeMax);
    }

    float diffTime = timeMax - timeMin;
    float progress = diffTime == 0.f ? 0.f : fabsf(reduct.v.values[Vertex::Time] - timeMin) / diffTime;

    bool ignore = (noOffsetAtEnds && (progress == 0 || progress == 1.f));

    if (cube->deformerAt(Vertex::Amp) >= 0) {
        int dfrm = cube->deformerAt(Vertex::Amp);
        noise.vertOffset  = vertOffsetSeeds [dfrm];
        noise.phaseOffset = phaseOffsetSeeds[dfrm];

        if (!ignore) {
            icpt.y += cube->deformerAbsGain(Vertex::Amp) * deformer->getTableValue(dfrm, progress, noise);
            NumberUtils::constrain(icpt.y, (scalingType != Unipolar ? -1.f : 0.f), 1.f);
        }
    }

    if (cube->deformerAt(Vertex::Phase) >= 0) {
        int dfrm = cube->deformerAt(Vertex::Phase);
        noise.vertOffset  = vertOffsetSeeds [dfrm];
        noise.phaseOffset = phaseOffsetSeeds[dfrm];

        if (!ignore) {
            icpt.adjustedX += cube->deformerAbsGain(Vertex::Phase) * deformer->getTableValue(dfrm, progress, noise);

            if (cyclic) {
                float lastAdjX = icpt.adjustedX;

                while(icpt.adjustedX >= 1.f)     { icpt.adjustedX -= 1.f; }
                while(icpt.adjustedX < 0.f)     { icpt.adjustedX += 1.f; }

                if (lastAdjX != icpt.adjustedX) {
                    icpt.isWrapped = true;
                    needsResorting = true;
                }
            }
        }
    }

    if (cube->deformerAt(Vertex::Curve) >= 0) {
        int dfrm = cube->deformerAt(Vertex::Curve);
        noise.vertOffset  = vertOffsetSeeds[dfrm];
        noise.phaseOffset = phaseOffsetSeeds[dfrm];

        icpt.shp += 2 * cube->deformerAbsGain(Vertex::Curve) * deformer->getTableValue(dfrm, progress, noise);

        NumberUtils::constrain(icpt.shp, 0.f, 1.f);
    }
}

void MeshRasterizer::handleOtherOverlappingLines(Vertex2 a, Vertex2 b, VertCube* cube) {
}


void MeshRasterizer::wrapVertices(float& ax, float& ay,
                                  float& bx, float& by, float indie) {
    if (cyclic) {
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
}

void MeshRasterizer::cleanUp() {
    waveX.nullify();
    waveY.nullify();

    colorPoints    .clear();

    // for when layer change and there are no intercepts in new layer, we don't want the old ones carrying over
    icpts        .clear();
    frontIcpts    .clear();
    backIcpts    .clear();
    deformRegions.clear();

    if(! batchMode) {
        makeCopy();
    }

    unsampleable = true;
}

void MeshRasterizer::setResolutionIndices(float base) {
    float dx;
    int res;
    int lastIdx = curves.size() - 1;

    for (int i = 1; i < curves.size() - 1; ++i) {
        dx = (curves[i + 1].c.x - curves[i - 1].a.x);

        for (int j = 0; j < Curve::resolutions; ++j) {
            res = Curve::resolution >> j;

            if (dx < base * float(res)) {
                curves[i].resIndex = j;
            }
        }
    }

    int padding = getPaddingSize();

    curves.front().resIndex = curves[lastIdx - 2 * (padding - 1)].resIndex;
    curves.back().resIndex = curves[2 * padding - 1].resIndex;
}


void MeshRasterizer::preCleanup() {
}


MeshRasterizer& MeshRasterizer::operator=(const MeshRasterizer& copy) {
//    jassertfalse;

    this->xMinimum                 = copy.xMinimum;
    this->xMaximum                 = copy.xMaximum;
    this->mesh                     = copy.mesh;
    this->morph                     = copy.morph;
    this->lowResCurves             = copy.lowResCurves;
    this->scalingType             = copy.scalingType;
    this->name                     = copy.name;
    this->zeroIndex             = 0;
    this->oneIndex                 = INT_MAX / 2;

    // flags
    this->overrideDim             = copy.overrideDim;
    this->overridingDim         = copy.overridingDim;
    this->cyclic                 = copy.cyclic;
    this->deformer                 = copy.deformer;
    this->calcInterceptsOnly     = copy.calcInterceptsOnly;
    this->decoupleComponentDfrms= copy.decoupleComponentDfrms;

    this->integralSampling         = copy.integralSampling;
    this->paddingSize             = copy.paddingSize;
    this->interceptPadding         = copy.interceptPadding;
    this->needsResorting         = copy.needsResorting;
    this->interpolateCurves     = copy.interpolateCurves;
    this->unsampleable             = true;

    return *this;
}

MeshRasterizer::MeshRasterizer(const MeshRasterizer& copy) {
//    jassertfalse;

    operator=(copy);
    initialise();
}

void MeshRasterizer::separateIntercepts(vector<Intercept>& intercepts, float minDx) {
    float cumulativeRetract = 0;
    float retractThisTime = 0;

    int size = intercepts.size();

    for (int i = 0; i < size - 1; ++i) {
        retractThisTime = jmax(0.f, intercepts[i].x + minDx - intercepts[i + 1].x);
        cumulativeRetract += retractThisTime;
    }

    // if we don't have room to space out the icpts, we need to reduce their volume
    bool doFlatten = false;

    if (cumulativeRetract > intercepts[0].x) {
        float endSpace = (1 - intercepts.back().x) - minDx;
        float maxSpread = intercepts[0].x + endSpace;

        for(int i = 0; i < size; ++i) {
            intercepts[i].x += endSpace;
        }

        if(cumulativeRetract > maxSpread) {
            doFlatten = true;
        }

        cumulativeRetract = intercepts.front().x;
    }

    float mostMovable = doFlatten ? cumulativeRetract / (float)(intercepts.size() + 1) : minDx;

    for (int i = 0; i < size - 1; ++i) {
        retractThisTime     = jmax(0.f, intercepts[i].x + mostMovable - intercepts[i + 1].x);
        intercepts[i].x     -= cumulativeRetract;
        cumulativeRetract     -= retractThisTime;
    }
}

void MeshRasterizer::oversamplingChanged() {
}

#pragma push_macro("new")
#undef new

void MeshRasterizer::initialise() {
    std::memset(vertOffsetSeeds, 0, numElementsInArray(vertOffsetSeeds) * sizeof(short));
    std::memset(phaseOffsetSeeds, 0, numElementsInArray(phaseOffsetSeeds) * sizeof(short));

    calcTransferTable();
    updateBuffers(2048);
}

#pragma pop_macro("new")

void MeshRasterizer::performUpdate(UpdateType updateType) {
    if (updateType == Update) {
        calcCrossPoints();
        makeCopy();
    }
}

int MeshRasterizer::getNumDims() {
    return 3;
}

bool MeshRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumCubes() > 1;
}

bool MeshRasterizer::isSampleable() {
    return !waveX.empty();
}

bool MeshRasterizer::isSampleableAt(float x) {
    return !waveX.empty() && waveX.front() <= x && waveX.back() > x;
}

Mesh* MeshRasterizer::getCrossPointsMesh() {
    return mesh;
}

void MeshRasterizer::print(OutputStream& dout) {
    dout << "time: " << morph.time << "\n";
    dout << "key: " << morph.red << "\n";
    dout << "mod: " << morph.blue << "\n";
    dout << "wrap ends: " << cyclic << "\n";
    dout << "low res: " << lowResCurves << "\n";
    dout << "\n";

    dout << "intercepts: " << "\n";
    for(auto & icpt : icpts) {
        dout << icpt.x << "\t" << icpt.y << "\n";
    }

    dout << "\n";
}

void MeshRasterizer::updateBuffers(int size) {
    const int numBuffers = 5;
    memoryBuffer.ensureSize(size * numBuffers);

    waveX     = memoryBuffer.place(size);
    waveY     = memoryBuffer.place(size);
    diffX     = memoryBuffer.place(size);
    slope     = memoryBuffer.place(size);
    area    = memoryBuffer.place(size);
}

void MeshRasterizer::makeCopy() {
//    lockTrace(&rastArrays.lock);
    ScopedLock sl(rastArrays.lock);

    int size = waveX.size();

    rastArrays.intercepts     = icpts;
    rastArrays.colorPoints    = colorPoints;
    rastArrays.curves         = curves;

    if (size > 0) {
        rastArrays.buffer.ensureSize(size * 2);

        rastArrays.waveX      = rastArrays.buffer.place(size);
        rastArrays.waveY      = rastArrays.buffer.place(size);
        rastArrays.oneIndex  = oneIndex;
        rastArrays.zeroIndex = zeroIndex;

        waveX.copyTo(rastArrays.waveX);
        waveY.copyTo(rastArrays.waveY);
    } else {
        rastArrays.waveX.nullify();
        rastArrays.waveY.nullify();
        rastArrays.oneIndex = 0;
        rastArrays.zeroIndex = 0;
    }
}

void MeshRasterizer::restoreStateFrom(RenderState& src) {
    lowResCurves         = src.lowResCurves;
    calcDepthDims         = src.calcDepthDims;
    morph                = src.pos;
    scalingType            = src.scalingType;
    batchMode            = src.batchMode;
}

void MeshRasterizer::saveStateTo(RenderState& dst) {
    dst.lowResCurves     = lowResCurves;
    dst.calcDepthDims     = calcDepthDims;
    dst.pos                = morph;
    dst.scalingType        = scalingType;
    dst.batchMode        = batchMode;
}

void MeshRasterizer::updateValue(int dim, float value) {
    switch (dim) {
        case CommonEnums::YellowDim:     morph.time  = value;    break;
        case CommonEnums::RedDim:         morph.red     = value;    break;
        case CommonEnums::BlueDim:         morph.blue     = value;    break;
        default: break;
    }
}

void MeshRasterizer::updateOffsetSeeds(int layerSize, int tableSize) {
    Random rand(Time::currentTimeMillis());

    for (int i = 0; i < layerSize; ++i) {
        vertOffsetSeeds[i]  = rand.nextInt(tableSize);
        phaseOffsetSeeds[i] = rand.nextInt(tableSize);
    }
}

