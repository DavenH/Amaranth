#include <iterator>
#include <climits>
#include "GuideCurveProvider.h"
#include "Mesh.h"
#include "MeshRasterizer.h"

#include "VertCube.h"
#include "Rasterization/Policies/ComponentGuideSharpnessPolicy.h"
#include "Rasterization/Policies/DepthProjectionPolicy.h"
#include "Rasterization/Policies/GuideCurvePolicy.h"
#include "Rasterization/Policies/InterceptRestrictionPolicy.h"
#include "Rasterization/Policies/PaddingPolicy.h"
#include "Rasterization/Policies/PointScalingPolicy.h"
#include "Rasterization/Builders/TransferTable.h"
#include "Rasterization/Facades/MeshRasterizerFacade.h"
#include "Rasterization/RasterizerComposer.h"
#include "Rasterization/Sampling/GuideCurveSampler.h"
#include "Rasterization/Sampling/WaveformSampler.h"
#include "Rasterization/Sources/MeshCubeSource.h"
#include "../App/AppConstants.h"
#include "../App/MeshLibrary.h"
#include "../Array/ScopedAlloc.h"
#include "../Design/Updating/Updater.h"
#include "../Util/CommonEnums.h"
#include "../Util/Util.h"

Rasterization::ScopedMeshRasterizerRenderState::ScopedMeshRasterizerRenderState(
        MeshRasterizer* rasterizer,
        MeshRasterizerRenderState* state) :
        rasterizer(rasterizer)
    ,   state(state) {
    rasterizer->saveStateTo(*state);
}

Rasterization::ScopedMeshRasterizerRenderState::~ScopedMeshRasterizerRenderState() {
    rasterizer->restoreStateFrom(*state);
}

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
    runtime.waveform        = createWaveformRefs();
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
    if (!canRasterizeMesh(usedMesh)) {
        cleanUp();
        return;
    }

    beginCrossPointCalculation();

    Rasterization::MeshSlicePipeline::Output output = renderMeshSlice(usedMesh, oscPhase);
    publishMeshSliceOutput(output);
    finishCrossPointCalculation();
}

Rasterization::MeshSlicePipeline::Output MeshRasterizer::renderMeshSlice(Mesh* usedMesh, float oscPhase) {
    Rasterization::MeshCubeSource source(usedMesh);
    Rasterization::RasterizationRequest request = createRasterizationRequest();

    auto composedRasterizer = Rasterization::RasterizerComposer::mesh()
            .withSource(source)
            .withRequest(request)
            .build();

    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();

    const auto& output = composedRasterizer.renderWithReduction(
            oscPhase,
            guideApplier,
            reduct);

    return output;
}

bool MeshRasterizer::canRasterizeMesh(Mesh* usedMesh) const {
    return usedMesh != nullptr && usedMesh->getNumCubes() > 0;
}

void MeshRasterizer::beginCrossPointCalculation() {
    icpts.clear();
    preCleanup();

    needsResorting = false;
}

void MeshRasterizer::publishMeshSliceOutput(const Rasterization::MeshSlicePipeline::Output& output) {
    icpts = output.intercepts;

    if (calcDepthDims) {
        colorPoints = output.colorPoints;
    }
}

void MeshRasterizer::finishCrossPointCalculation() {
    processIntercepts(icpts);
    sortInterceptsIfNeeded();

    int end = icpts.size() - 1;
    if (end < 0) {
        cleanUp();

        return;
    }

    if (end == 0) {
        curves.clear();

        markWaveformUnsampleable();
    }

    Rasterization::MeshSlicePipeline::applyPaddingFlags(icpts);

    if (!calcInterceptsOnly) {
        rebuildCurvesFromIntercepts();
    }
}

void MeshRasterizer::sortInterceptsIfNeeded() {
    if (Util::assignAndWereDifferent(needsResorting, false)) {
        std::sort(icpts.begin(), icpts.end());
    }
}

void MeshRasterizer::rebuildCurvesFromIntercepts() {
    curves.clear();

    if (cyclic) {
        padIcptsWrapped(icpts, curves);
    } else {
        padIcpts(icpts, curves);
    }

    updateCurves();
}

int MeshRasterizer::getPrimaryViewDimension() {
    return Vertex::Time;
}

void MeshRasterizer::calcIntercepts() {
    ScopedValueSetter calcIcptsFlag(calcInterceptsOnly, true, calcInterceptsOnly);

    calcCrossPoints();
    makeCopy();
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
    Rasterization::ComponentGuideSharpnessPolicy().apply(curves);
}

void MeshRasterizer::updateCurves() {
    if (icpts.size() < 2) {
        return;
    }

    Rasterization::CurveResolutionPolicy::Context resolutionContext = createCurveResolutionContext();
    facade->applyCurveResolution(curves, resolutionContext);

    adjustDeformingSharpness();

    for (auto & curve : curves) {
        curve.recalculateCurve();
    }

    calcWaveform();
}

Rasterization::CurveResolutionPolicy::Context MeshRasterizer::createCurveResolutionContext() const {
    Rasterization::CurveResolutionPolicy::Context context;
    context.lowResCurves = lowResCurves;
    context.integralSampling = integralSampling;
    context.interpolateCurves = interpolateCurves;
    context.paddingSize = paddingSize;

    return context;
}

void MeshRasterizer::calcWaveform() {
    if(decoupleComponentDfrms) {
        guideCurveRegions.clear();
    }

    Rasterization::WaveformBakePolicy::Context context = createWaveformBakeContext();

    int totalRes = facade->prepareWaveform(curves, context);
    updateBuffers(totalRes);

    context.waveform = createWaveformRefs();
    facade->bakeWaveform(curves, context);

    unsampleable = false;
}

Rasterization::WaveformBakePolicy::Context MeshRasterizer::createWaveformBakeContext() {
    Rasterization::WaveformBakePolicy::Context context;
    context.lowResCurves = lowResCurves;
    context.decoupleComponentDfrms = decoupleComponentDfrms;
    context.noiseSeed = noiseSeed;
    context.morph = morph;
    context.guideCurveProvider = guideCurveProvider;
    context.guideCurveRegions = &guideCurveRegions;
    context.offsetSeeds = &guideCurveOffsetSeeds;
    context.transferTable = Rasterization::TransferTable::values();

    return context;
}

float MeshRasterizer::sampleAt(double angle) {
    return Rasterization::WaveformSampler::sampleAt(
            createWaveformView(),
            unsampleable,
            angle);
}

/*
 * For single sample per cycle rasterization, e.g. envelopes
 */
float MeshRasterizer::sampleAtDecoupled(double angle, GuideCurveContext& context) {
    return Rasterization::GuideCurveSampler::sampleDecoupled(
            createWaveformView(),
            unsampleable,
            angle,
            context,
            guideCurveRegions,
            guideCurveProvider,
            noiseSeed);
}

// make damn sure that the last element in waveX is greater than 1 before calling this
float MeshRasterizer::sampleAt(double angle, int& currentIndex) {
    return Rasterization::WaveformSampler::sampleAt(
            createWaveformView(),
            unsampleable,
            angle,
            currentIndex);
}


void MeshRasterizer::sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) {
    Rasterization::WaveformSampler::sampleAtIntervals(
            createWaveformView(),
            intervals,
            dest);
}


float MeshRasterizer::samplePerfectly(double delta, Buffer<float> buffer, double phase) {
    return Rasterization::WaveformSampler::samplePerfectly(
            createWaveformView(),
            delta,
            buffer,
            phase);
}

void MeshRasterizer::reset() {
    curves.clear();
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();

    colorPoints.clear();

    markWaveformUnsampleable();
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
        markWaveformUnsampleable();

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
    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();
    guideApplier(icpt, morph, noOffsetAtEnds);
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
    markWaveformUnsampleable();

    colorPoints.clear();

    // for when layer change and there are no intercepts in new layer, we don't want the old ones carrying over
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();
    guideCurveRegions.clear();

    if(! batchMode) {
        makeCopy();
    }

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
    guideCurveOffsetSeeds.reset();

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
    return Rasterization::WaveformSampler::isSampleable(createWaveformView());
}

bool MeshRasterizer::isSampleableAt(float x) {
    return Rasterization::WaveformSampler::isSampleableAt(createWaveformView(), x);
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

void MeshRasterizer::markWaveformUnsampleable() {
    waveX.nullify();
    waveY.nullify();

    unsampleable = true;
}

Rasterization::WaveformBuffers MeshRasterizer::createWaveformView() const {
    return Rasterization::WaveformBuffers(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex);
}

Rasterization::WaveformBufferRefs MeshRasterizer::createWaveformRefs() {
    return Rasterization::WaveformBufferRefs(
            waveX,
            waveY,
            diffX,
            slope,
            area,
            zeroIndex,
            oneIndex);
}

void MeshRasterizer::assignWaveform(const Rasterization::WaveformBuffers& waveform) {
    createWaveformRefs().assignFrom(waveform);
}

void MeshRasterizer::copyWaveform(const Rasterization::WaveformBuffers& waveform) {
    waveform.waveX.copyTo(waveX);
    waveform.waveY.copyTo(waveY);
    waveform.diffX.copyTo(diffX);
    waveform.slope.copyTo(slope);
    waveform.area.copyTo(area);
    zeroIndex = waveform.zeroIndex;
    oneIndex = waveform.oneIndex;
}

Rasterization::GuideCurvePolicyContext MeshRasterizer::createGuideCurvePolicyContext() {
    Rasterization::GuideCurvePolicyContext context;
    context.guideCurveProvider = guideCurveProvider;
    context.reduction = &reduct;
    context.scalingMode = Rasterization::pointScalingModeFromLegacy(scalingType);
    context.cyclic = cyclic;
    context.needsResorting = &needsResorting;
    context.noiseSeed = noiseSeed;
    context.offsetSeeds = &guideCurveOffsetSeeds;

    return context;
}

Rasterization::GuideCurveApplier MeshRasterizer::createGuideCurveApplier() {
    return Rasterization::GuideCurveApplier(createGuideCurvePolicyContext());
}

void MeshRasterizer::makeCopy() {
    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &icpts;
    source.colorPoints = &colorPoints;
    source.curves = &curves;
    source.waveform = createWaveformView();

    facade->publishSnapshot(rastArrays, source);
}

void MeshRasterizer::restoreStateFrom(RenderState& src) {
    lowResCurves         = src.lowResCurves;
    calcDepthDims         = src.calcDepthDims;
    morph                = src.pos;
    scalingType            = (ScalingType) src.scalingType;
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
    guideCurveOffsetSeeds.randomize(layerSize, tableSize, rand);
}
