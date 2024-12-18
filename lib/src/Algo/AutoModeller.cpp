#include <fstream>
#include "AutoModeller.h"
#include "Resampling.h"
#include "Fir.h"
#include "../Curve/Curve.h"
#include "../Curve/Rasterizer2D.h"
#include "../Design/Updating/Updater.h"
#include "../Inter/Interactor2D.h"
#include "../UI/Panels/Panel.h"
#include "PeakCounter.h"

AutoModeller::AutoModeller() :
        rasterizer			(points, false)
    ,	random				(Time::currentTimeMillis())
    ,	leftSamplingOffset	(0.f)
    ,	rightSamplingOffset	(1.f)
    ,	reductionLevel		(0.3f)
    ,	useInflections		(false) {
}

void AutoModeller::amplifyCloseVerts() {
    // amplify close vertices
    vector<Intercept> origPoints = points;
    float dx, dy, dyA, dyB;

    for (int i = 1; i < (int) points.size() - 1; ++i) {
        dx = origPoints[i + 1].x - origPoints[i - 1].x;

        float factor = dx < 0.1f ? 2.f : dx < 0.2f ? 1.3f : 1.f;

        dyA = origPoints[i].y - origPoints[i - 1].y;
        dyB = origPoints[i].y - origPoints[i + 1].y;
        dy 	= fabsf(dyA) < fabsf(dyB) ? dyA : dyB;

        points[i].y += factor * dy;
    }
}

float AutoModeller::performFitness(int curveIdx, const Intercept& icpt,
                                   int sampleStart, int sampleSize,
                                   float invLength, float left, bool doUpdate) {
    if(doUpdate) {
        rasterizer.updateCurve(curveIdx, icpt);
    }

    Buffer<float> rastBuf = rastMem.section(sampleStart, sampleSize);
    Buffer<float> sampBuf = srcSamples.section(sampleStart, sampleSize);

    rasterizer.sampleWithInterval(rastBuf, invLength, left);
    float error = sampBuf.normDiffL2(rastBuf) / float(sampleSize);

    return error;
}

void AutoModeller::preparePoints() {
    if(srcSamples.empty()) {
        return;
    }

    int length = srcSamples.size();

    ScopedAlloc<float> memory(length * 2);

    Buffer<float> filtered = memory.place(length);
    Buffer<float> ramp = memory.place(length);
    FIR fir(0.3, 16);

    srcSamples.copyTo(filtered);
    fir.process(filtered);

    float left = leftSamplingOffset;
    float delta = (rightSamplingOffset - leftSamplingOffset) / float(length);
    ramp.ramp(left, delta);

    if (useInflections) {
        vector<int> inflections = PeakCounter::findInflections(filtered);

        if(inflections.empty()) {
            return;
        }

        points.clear();

        for(int inflection : inflections)
            points.emplace_back(ramp[inflection], filtered[inflection]);

        points = reducePath(points, reductionLevel);
    } else {
        vector<Intercept> spoints;
        for(int i = 0; i < filtered.size(); ++i)
            spoints.emplace_back(ramp[i], srcSamples[i]);

        points = reducePath(spoints, reductionLevel);
    }

    if (points.size() > 1 && !rasterizer.isCyclic()) {
        points.front().shp = 0.8f;
        points.back().shp = 0.8f;
    }

    rasterizer.performUpdate(Update);
    rasterizer.validateCurves();
    rasterizer.sampleWithInterval(rastMem, delta, left);

    fit();
}

vector<Intercept> AutoModeller::modelToPath(vector<Vertex2>& path, float reduction, bool useInflections) {
    if(path.size() < 2)
        return {};

    srcSamples.resize(1024);
    rastMem.ensureSize(1024);
    rasterizer.setCyclicity(false);

    this->useInflections = useInflections;

    int currentIndex 	= 1;
    reductionLevel 		= reduction;
    leftSamplingOffset 	= path.front().x;
    rightSamplingOffset = path.back().x;
    float span 			= rightSamplingOffset - leftSamplingOffset;

    std::sort(path.begin(), path.end());

    float invSize = 1 / float(srcSamples.size() - 1);
    for(int i = 0; i < srcSamples.size(); ++i) {
        float x = leftSamplingOffset + span * i * invSize;
        srcSamples[i] = Resampling::at(x, path, currentIndex);
    }

    preparePoints();

    return points;
}

void AutoModeller::modelToInteractor(
        const Buffer<float> &buffer,
        Interactor2D *interactor,
        bool isCyclic,
        float leftOffset,
        float reduction) {
    if(buffer.empty()) {
        return;
    }

    leftSamplingOffset 	= leftOffset;
    reductionLevel 		= reduction;
    useInflections 		= false;

    rasterizer.setCyclicity(isCyclic);
    srcSamples.resize(buffer.size());
    rastMem.ensureSize(buffer.size());

    buffer.copyTo(srcSamples);

    preparePoints();

    float startTime = interactor->getYellow();

    if (interactor->getMesh()) {
        interactor->setSuspendUndo(true);
        interactor->removeLinesInRange(Range(0.f, 1.f), interactor->getOffsetPosition(true));

        float y;
        for (auto& point : points) {
            y = point.y * 0.5f + 0.5f;
            NumberUtils::constrain<float>(y, 0.f, 1.f);

            interactor->addNewCube(startTime, point.x, y, point.shp);
        }

        interactor->setSuspendUndo(false);
    }
}

void AutoModeller::removeUselessPoints() {
    int size = (int) points.size();

    int totalSize = size * 5 - 3;
    ScopedAlloc<Ipp32f> memory(totalSize);

    Buffer<float> x 	= memory.place(size);
    Buffer<float> y 	= memory.place(size);
    Buffer<float> dx 	= memory.place(size - 1);
    Buffer<float> dy 	= memory.place(size - 1);
    Buffer<float> score = memory.place(size - 1);

    for(int i = 0; i < size; ++i)
    {
        x[i] = points[i].x;
        y[i] = points[i].y;
    }

    dx.sub(x, x + 1).sqr().threshLT(0.0001).inv();
    dy.sub(y, y + 1).abs().threshLT(0.0001).inv();
    score.mul(dx, dy).mul(0.0001f).sqrt();

    vector<Intercept> relevantPoints;
    relevantPoints.push_back(points.front());

    for(int i = 0; i < score.size(); ++i) {
        if(score[i] < reductionLevel) {
            relevantPoints.push_back(points[i + 1]);
        }
    }

    points = relevantPoints;
}

void AutoModeller::fit() {
    static const float varDecay 	= 0.75f;
    static const int flipflopIters 	= 3; //3;
    static const int convergeIters 	= 5;
    static const float variaY 		= 0.35f;
    static const float variaX 		= 0.15f;
    static const float variaShrp 	= 0.5f;

    for(int p = 1; p < (int) points.size() - 1; ++p) {
        const Intercept& prev 	= points[p - 1];
        Intercept& icpt 		= points[p + 0];
        const Intercept& next 	= points[p + 1];

        int curveIdx = p + rasterizer.getPaddingSize();

        float left  = prev.x + 0.00001f;
        float right = next.x - 0.00001f;

        double invLength 	= (rightSamplingOffset - leftSamplingOffset) / double(srcSamples.size());
        int sampleStart 	= jmax(0, (int) floorf((left - leftSamplingOffset) / invLength) - 4);
        int sampleEnd 		= jmin(srcSamples.size() - 1, (int) ceilf((right - leftSamplingOffset) / invLength) + 4);
        int sampleSize 		= sampleEnd - sampleStart;
        double sampleOffset = sampleStart * invLength;
        float sampleLeft    = leftSamplingOffset + sampleOffset;

        Buffer<float> rastBuf = rastMem.section(sampleStart, sampleSize);
        Buffer<float> sampBuf = srcSamples.section(sampleStart, sampleSize);

        float minFitness 	= performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft, false);
        float fitness 		= minFitness;

        Intercept best 		= icpt;
        float variationY 	= variaY * jmin(1.f, 5 * minFitness);
        float variationX 	= variaX * jmin(1.f, 5 * minFitness);
        float varSharp 		= variaShrp * jmin(1.f, 5 * minFitness);
        float deltaX 		= right - left;

        for (int k = 0; k < flipflopIters; ++k) {
            for (int i = 0; i < convergeIters; ++i) {
                float savedY 	= icpt.y;
                icpt.y 		 	= jmin(1.f, best.y + (2 * random.nextFloat() - 1) * variationY);
                fitness 	 	= performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);

                if (fitness < minFitness) {
                    best.y 		= icpt.y;
                    minFitness 	= fitness;
                } else {
                    icpt.y = savedY;
                    performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);
                }
            }

            for (int i = 0; i < convergeIters; ++i) {
                float savedX  	= icpt.x;
                icpt.x 			= jlimit(left, right, best.x + (2 * random.nextFloat() - 1) * variationX * deltaX);
                fitness 		= performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);

                if(fitness < minFitness) {
                    best.x 		= icpt.x;
                    minFitness 	= fitness;
                } else {
                    icpt.x = savedX;
                    performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);
                }
            }

            for (int i = 0; i < convergeIters; ++i) {
                float savedShp 	= icpt.shp;
                icpt.shp 		= jlimit(0.f, 1.f, best.shp + (2 * random.nextFloat() - 1) * varSharp);
                fitness 		= performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);

                if(fitness < minFitness) {
                    best.shp 	= icpt.shp;
                    minFitness 	= fitness;
                } else {
                    icpt.shp = savedShp;
                    performFitness(curveIdx, icpt, sampleStart, sampleSize, invLength, sampleLeft);
                }
            }
        }
    }
}