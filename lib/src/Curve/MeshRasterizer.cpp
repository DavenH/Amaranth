#include <iterator>
#include <cmath>
#include <climits>
#include "GuideCurveProvider.h"
#include "Mesh.h"
#include "MeshRasterizer.h"

#include "VertCube.h"
#include "Rasterization/Policies/DepthProjectionPolicy.h"
#include "Rasterization/Policies/InterceptRestrictionPolicy.h"
#include "Rasterization/Policies/PaddingPolicy.h"
#include "Rasterization/Policies/PointScalingPolicy.h"
#include "Rasterization/Builders/TransferTable.h"
#include "Rasterization/Facades/MeshRasterizerFacade.h"
#include "Rasterization/RasterizerComposer.h"
#include "Rasterization/Sources/MeshCubeSource.h"
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
    ,    guideCurveProvider  (nullptr)

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
    ,    batchMode           (false)
    ,    facade              (new Rasterization::MeshRasterizerFacade) {
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

Rasterization::RasterizationRequest MeshRasterizer::createRasterizationRequest() {
    Rasterization::RasterizationRequest request;
    request.dims                      = dims;
    request.morph                     = morph;
    request.scalingMode               = Rasterization::pointScalingModeFromLegacy(scalingType);
    request.batchMode                 = batchMode;
    request.calcDepthDimensions       = calcDepthDims;
    request.calcInterceptsOnly        = calcInterceptsOnly;
    request.cyclic                    = cyclic;
    request.decoupleComponentDeforms  = decoupleComponentDfrms;
    request.integralSampling          = integralSampling;
    request.interpolateCurves         = interpolateCurves;
    request.lowResCurves              = lowResCurves;
    request.overrideDimension         = overrideDim;
    request.publishSnapshot           = !batchMode;
    request.noiseSeed                 = noiseSeed;
    request.overridingDimension       = overridingDim;
    request.primaryViewDimension      = overrideDim ? overridingDim : getPrimaryViewDimension();
    request.paddingSize               = paddingSize;
    request.interceptPadding          = interceptPadding;
    request.xMinimum                  = xMinimum;
    request.xMaximum                  = xMaximum;

    return request;
}

Rasterization::RasterizerRuntime MeshRasterizer::createRasterizerRuntime() {
    Rasterization::RasterizerRuntime runtime;
    runtime.intercepts      = &icpts;
    runtime.curves          = &curves;
    runtime.frontPadding    = &frontIcpts;
    runtime.backPadding     = &backIcpts;
    runtime.colorPoints     = &colorPoints;
    runtime.waveform        = Rasterization::WaveformBufferRefs(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex);
    runtime.paddingSize     = &paddingSize;
    runtime.unsampleable    = &unsampleable;
    runtime.needsResorting  = &needsResorting;

    return runtime;
}

void MeshRasterizer::calcWaveformFrom(vector<Intercept>& icpts) {
    this->icpts = icpts;

    padIcpts(icpts, curves);
    updateCurves();
    makeCopy();
}

void MeshRasterizer::calcCrossPoints(Mesh* usedMesh, float oscPhase) {
    if (usedMesh == nullptr || usedMesh->getNumCubes() == 0) {
        cleanUp();
        return;
    }

    icpts.clear();
    preCleanup();

    needsResorting = false;

    Rasterization::MeshCubeSource source(usedMesh);
    Rasterization::RasterizationRequest request = createRasterizationRequest();

    auto composedRasterizer = Rasterization::RasterizerComposer::mesh()
            .withSource(source)
            .withRequest(request)
            .build();

    const auto& output = composedRasterizer.renderWithReduction(
            oscPhase,
            [this](Intercept& point, const MorphPosition& position, bool noOffsetAtEnds) {
                applyGuideCurves(point, position, noOffsetAtEnds);
            },
            reduct);

    icpts = output.intercepts;

    if (calcDepthDims) {
        colorPoints = output.colorPoints;
    }

    processIntercepts(icpts);

    if(Util::assignAndWereDifferent(needsResorting, false)) {
        std::sort(icpts.begin(), icpts.end());
    }

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

    Rasterization::MeshSlicePipeline::applyPaddingFlags(icpts);

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

    for (int i = 0; i < Curve::resolution; ++i) {
        transferTable[i] = Rasterization::TransferTable::values()[i];
    }

    alreadyCalculated = true;
}

void MeshRasterizer::restrictIntercepts(vector<Intercept>& intercepts) {
    if (intercepts.empty()) {
        return;
    }

    Rasterization::InterceptRestrictionPolicy::Context context;
    context.cyclic = cyclic;
    context.minimumX = xMinimum;
    context.maximumX = xMaximum;

    Rasterization::InterceptRestrictionPolicy(context).restrict(intercepts);

  #ifdef _DEBUG
    for(int i = 0; i < intercepts.size() - 1; ++i)
    {
        jassert(intercepts[i].x < intercepts[i + 1].x);
    }
  #endif
}

/*
 * set curve after a amp-vs-phase (component curve) guide curve, to full sharpness
 * so that waveX is continuous. At < 1 sharpness, the trailing
 * x-values of the curve create a discontinuity
 */
void MeshRasterizer::adjustDeformingSharpness() {
    for (int i = 0; i < (int) curves.size(); ++i) {
        Curve& curve = curves[i];

        if (i < (int) curves.size() - 1 && curve.b.cube != nullptr) {
            if (curve.b.cube->getCompGuideCurve() >= 0) {
                Curve& next = curves[i + 1];

                if (next.b.cube == nullptr || next.b.cube->getCompGuideCurve() < 0) {
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

    Rasterization::CurveResolutionPolicy::Context resolutionContext;
    resolutionContext.lowResCurves = lowResCurves;
    resolutionContext.integralSampling = integralSampling;
    resolutionContext.interpolateCurves = interpolateCurves;
    resolutionContext.paddingSize = paddingSize;
    facade->applyCurveResolution(curves, resolutionContext);

    adjustDeformingSharpness();

    for (auto & curve : curves) {
        curve.recalculateCurve();
    }

    calcWaveform();
}

void MeshRasterizer::calcWaveform() {
    if(decoupleComponentDfrms) {
        guideCurveRegions.clear();
    }

    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = lowResCurves;
    context.decoupleComponentDfrms = decoupleComponentDfrms;
    context.noiseSeed = noiseSeed;
    context.morph = morph;
    context.guideCurveProvider = guideCurveProvider;
    context.guideCurveRegions = &guideCurveRegions;
    context.phaseOffsetSeeds = phaseOffsetSeeds;
    context.vertOffsetSeeds = vertOffsetSeeds;
    context.transferTable = transferTable;

    int totalRes = facade->prepareWaveform(curves, context);
    updateBuffers(totalRes);

    context.waveform = Rasterization::WaveformBufferRefs(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex);
    facade->bakeWaveform(curves, context);

    unsampleable = false;
}

float MeshRasterizer::sampleAt(double angle) {
    int currentIndex = Arithmetic::binarySearch(angle, waveX);
    return sampleAt(angle, currentIndex);
}

/*
 * For single sample per cycle rasterization, e.g. envelopes
 */
float MeshRasterizer::sampleAtDecoupled(double angle, GuideCurveContext& context) {
    float val = sampleAt(angle, context.currentIndex);

    for(auto& region : guideCurveRegions) {
        if (NumberUtils::within<float>(angle, region.start.x, region.end.x)) {
            float diff = region.end.x - region.start.x;

            if (diff > 0) {
                GuideCurveProvider::NoiseContext noise;
                noise.noiseSeed     = noiseSeed;
                noise.phaseOffset     = context.phaseOffsetSeed;
                noise.vertOffset     = context.vertOffsetSeed;

                float progress = (angle - region.start.x) / diff;

                return val + region.amplitude * guideCurveProvider->getTableValue(region.guideIndex, progress, noise);
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
    int end = intercepts.size() - 1;

    if (end < 1) {
        return;
    }

    Rasterization::PaddingPolicyContext context;
    context.interceptPadding = interceptPadding;

    paddingSize = Rasterization::CyclicPaddingPolicy(context).build(
            intercepts,
            curves,
            frontIcpts,
            backIcpts);
}

void MeshRasterizer::padIcpts(vector<Intercept>& intercepts, vector<Curve>& curves) {
    int end = intercepts.size() - 1;

    if (end == 0) {
        waveX.nullify();
        waveY.nullify();
        unsampleable = true;

        return;
    }

    Rasterization::PaddingPolicyContext context;
    context.minimumX = xMinimum;
    context.maximumX = xMaximum;
    context.boundsIntercepts = &icpts;

    paddingSize = Rasterization::NonCyclicPaddingPolicy(context).build(intercepts, curves);
}

void MeshRasterizer::validateCurves() {
    for (int i = 0; i < (int) curves.size() - 1; ++i) {
        jassert(curves[i].b.x == curves[i + 1].a.x);
        jassert(curves[i].c.x == curves[i + 1].b.x);
    }
}

// NB: set the intercept's adjustedX property rather than x
// it will be used to re-sort and then assign the x property
void MeshRasterizer::applyGuideCurves(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds) {
    if(icpt.cube == nullptr || guideCurveProvider == nullptr) {
        return;
    }

    VertCube* cube = icpt.cube;

    GuideCurveProvider::NoiseContext noise;
    noise.noiseSeed = noiseSeed;

    if (cube->guideCurveAt(Vertex::Red) >= 0) {
        int guideIndex = cube->guideCurveAt(Vertex::Red);
        float progress = cube->getPortionAlong(Vertex::Red, morph);

        noise.vertOffset  = vertOffsetSeeds [guideIndex];
        noise.phaseOffset = phaseOffsetSeeds[guideIndex];

        icpt.adjustedX += cube->guideCurveAbsGain(Vertex::Red) * guideCurveProvider->getTableValue(guideIndex, progress, noise);
    }

    if (cube->guideCurveAt(Vertex::Blue) >= 0) {
        int guideIndex = cube->guideCurveAt(Vertex::Blue);
        float progress = cube->getPortionAlong(Vertex::Blue, morph);

        noise.vertOffset  = vertOffsetSeeds [guideIndex];
        noise.phaseOffset = phaseOffsetSeeds[guideIndex];

        icpt.adjustedX += cube->guideCurveAbsGain(Vertex::Blue) * guideCurveProvider->getTableValue(guideIndex, progress, noise);
    }

    float timeMin = reduct.v0.values[Vertex::Time];
    float timeMax = reduct.v1.values[Vertex::Time];

    if(timeMin > timeMax) {
        std::swap(timeMin, timeMax);
    }

    float diffTime = timeMax - timeMin;
    float progress = diffTime == 0.f ? 0.f : fabsf(reduct.v.values[Vertex::Time] - timeMin) / diffTime;

    bool ignore = (noOffsetAtEnds && (progress == 0 || progress == 1.f));

    if (cube->guideCurveAt(Vertex::Amp) >= 0) {
        int guideIndex = cube->guideCurveAt(Vertex::Amp);
        noise.vertOffset  = vertOffsetSeeds [guideIndex];
        noise.phaseOffset = phaseOffsetSeeds[guideIndex];

        if (!ignore) {
            Rasterization::PointScalingPolicy scalingPolicy =
                    Rasterization::PointScalingPolicy::fromLegacyScalingType(scalingType);

            icpt.y += cube->guideCurveAbsGain(Vertex::Amp) * guideCurveProvider->getTableValue(guideIndex, progress, noise);
            NumberUtils::constrain(icpt.y, scalingPolicy.minimum(), scalingPolicy.maximum());
        }
    }

    if (cube->guideCurveAt(Vertex::Phase) >= 0) {
        int guideIndex = cube->guideCurveAt(Vertex::Phase);
        noise.vertOffset  = vertOffsetSeeds [guideIndex];
        noise.phaseOffset = phaseOffsetSeeds[guideIndex];

        if (!ignore) {
            icpt.adjustedX += cube->guideCurveAbsGain(Vertex::Phase) * guideCurveProvider->getTableValue(guideIndex, progress, noise);

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

    if (cube->guideCurveAt(Vertex::Curve) >= 0) {
        int guideIndex = cube->guideCurveAt(Vertex::Curve);
        noise.vertOffset  = vertOffsetSeeds[guideIndex];
        noise.phaseOffset = phaseOffsetSeeds[guideIndex];

        icpt.shp += 2 * cube->guideCurveAbsGain(Vertex::Curve) * guideCurveProvider->getTableValue(guideIndex, progress, noise);

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

    colorPoints.clear();

    // for when layer change and there are no intercepts in new layer, we don't want the old ones carrying over
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();
    guideCurveRegions.clear();

    if(! batchMode) {
        makeCopy();
    }

    unsampleable = true;
}

void MeshRasterizer::setResolutionIndices(float base) {
    facade->applyResolutionIndices(curves, base, getPaddingSize());
}


void MeshRasterizer::preCleanup() {
}


MeshRasterizer& MeshRasterizer::operator=(const MeshRasterizer& copy) {
//    jassertfalse;
    if (facade == nullptr) {
        facade.reset(new Rasterization::MeshRasterizerFacade);
    }

    this->xMinimum               = copy.xMinimum;
    this->xMaximum               = copy.xMaximum;
    this->mesh                   = copy.mesh;
    this->morph                  = copy.morph;
    this->lowResCurves           = copy.lowResCurves;
    this->scalingType            = copy.scalingType;
    this->name                   = copy.name;
    this->zeroIndex              = 0;
    this->oneIndex               = INT_MAX / 2;

    // flags
    this->overrideDim            = copy.overrideDim;
    this->overridingDim          = copy.overridingDim;
    this->cyclic                 = copy.cyclic;
    this->guideCurveProvider     = copy.guideCurveProvider;
    this->calcInterceptsOnly     = copy.calcInterceptsOnly;
    this->decoupleComponentDfrms = copy.decoupleComponentDfrms;

    this->integralSampling       = copy.integralSampling;
    this->paddingSize            = copy.paddingSize;
    this->interceptPadding       = copy.interceptPadding;
    this->needsResorting         = copy.needsResorting;
    this->interpolateCurves      = copy.interpolateCurves;
    this->unsampleable           = true;

    return *this;
}

MeshRasterizer::MeshRasterizer(const MeshRasterizer& copy) :
        facade(new Rasterization::MeshRasterizerFacade) {
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
    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &icpts;
    source.colorPoints = &colorPoints;
    source.curves = &curves;
    source.waveform = Rasterization::WaveformBuffers(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex);

    facade->publishSnapshot(rastArrays, source);
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
