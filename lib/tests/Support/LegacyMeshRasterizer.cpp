#include <iterator>
#include <climits>

#include "LegacyMeshRasterizer.h"

#include <Curve/GuideCurveProvider.h>
#include <Curve/Mesh.h>
#include <Curve/VertCube.h>
#include <Curve/Rasterization/Policies/Curves/CurvePolicies.h>
#include <Curve/Rasterization/Policies/Mesh/DepthProjectionPolicy.h>
#include <Curve/Rasterization/Policies/Mesh/GuideCurvePolicy.h>
#include <Curve/Rasterization/Policies/Core/InterceptPolicies.h>
#include <Curve/Rasterization/Policies/Core/PaddingPolicy.h>
#include <Curve/Rasterization/Policies/Core/PointScalingPolicy.h>
#include <Curve/Rasterization/Builders/RasterizerSnapshotBuilder.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>
#include <Curve/Rasterization/Policies/Curves/WaveformBakePolicy.h>
#include <Curve/Rasterization/Sampling/GuideCurveSampler.h>
#include <Curve/Rasterization/Sampling/WaveformSampler.h>
#include <Util/CommonEnums.h>

MeshRasterizer::ScopedRenderState::ScopedRenderState(
        MeshRasterizer* rasterizer,
        RenderState* state) :
        rasterizer(rasterizer)
    ,   state(state) {
    rasterizer->saveStateTo(*state);
}

MeshRasterizer::ScopedRenderState::~ScopedRenderState() {
    rasterizer->restoreStateFrom(*state);
}

MeshRasterizer::MeshRasterizer(const String& name) :
         name                (name)
    ,    mesh                (nullptr)
    ,    guideCurveProvider  (nullptr)

    ,    paddingSize         (2)
    ,    noiseSeed           (-1)
    ,    overridingDim       (Vertex::Time)
    ,    meshSlicer()
    ,    meshSliceResult()

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
    ,    result()
    ,    rasterizerData()
    ,    rastArrays          (rasterizerData)
    ,    frontIcpts          (result.frontPadding)
    ,    backIcpts           (result.backPadding)
    ,    icpts               (result.intercepts)
    ,    colorPoints         (result.colorPoints)
    ,    curves              (result.curves)
    ,    guideCurveRegions   (result.guideCurveRegions)
    ,    waveform            (result.waveform) {
    initialise();
}


MeshRasterizer::~MeshRasterizer() {
    waveform.nullify();

    curves.clear();
    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();

    makeCopy();
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

void MeshRasterizer::calcCrossPoints(Mesh* usedMesh, float oscPhase) {
    if (!canRasterizeMesh(usedMesh)) {
        cleanUp();
        return;
    }

    beginCrossPointCalculation();

    const Rasterization::RenderResult& output = renderMeshSlice(usedMesh, oscPhase);
    publishMeshSliceOutput(output);
    finishCrossPointCalculation();
}

const Rasterization::RenderResult& MeshRasterizer::renderMeshSlice(Mesh* usedMesh, float oscPhase) {
    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();

    return meshSlicer.sliceMesh(
            usedMesh,
            createRasterizationRequest(),
            oscPhase,
            guideApplier,
            meshSliceResult,
            reduct);
}

bool MeshRasterizer::canRasterizeMesh(Mesh* usedMesh) const {
    return usedMesh != nullptr && usedMesh->getNumCubes() > 0;
}

void MeshRasterizer::beginCrossPointCalculation() {
    icpts.clear();

    needsResorting = false;
}

void MeshRasterizer::publishMeshSliceOutput(const Rasterization::RenderResult& output) {
    icpts = output.intercepts;

    if (calcDepthDims) {
        colorPoints = output.colorPoints;
    }
}

void MeshRasterizer::finishCrossPointCalculation() {
    sortInterceptsIfNeeded();

    if (handleDegenerateInterceptOutput()) {
        return;
    }

    Rasterization::InterceptPaddingFlagPolicy().apply(icpts);

    if (!calcInterceptsOnly) {
        rebuildCurvesFromIntercepts();
    }
}

bool MeshRasterizer::handleDegenerateInterceptOutput() {
    switch (Rasterization::InterceptDegeneracyPolicy().classify(icpts.size())) {
        case Rasterization::InterceptDegeneracyAction::CleanUp:
            cleanUp();
            return true;
        case Rasterization::InterceptDegeneracyAction::MarkUnsampleable:
            curves.clear();
            markWaveformUnsampleable();
            return false;
        case Rasterization::InterceptDegeneracyAction::Continue:
            return false;
    }

    return false;
}

void MeshRasterizer::sortInterceptsIfNeeded() {
    Rasterization::InterceptSortPolicy(&needsResorting).sortIfNeeded(icpts);
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

void MeshRasterizer::updateCurves() {
    if (icpts.size() < 2) {
        return;
    }

    Rasterization::CurveResolutionPolicy::Context resolutionContext = createCurveResolutionContext();
    Rasterization::CurveResolutionPolicy().apply(curves, resolutionContext);

    prepareCurvesForWaveform();
    calcWaveform();
}

void MeshRasterizer::prepareCurvesForWaveform() {
    Rasterization::CurveWaveformPreparationPolicy().apply(curves);
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

    bool sampleable = Rasterization::WaveformBakePolicy().build(
            curves,
            context,
            [this](int totalRes) {
                updateBuffers(totalRes);
                return Rasterization::WaveformBufferRefs(waveform);
            });

    unsampleable = !sampleable;
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
    clearRasterizationResult(true);
    guideCurveRegions.clear();
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

// NB: set the intercept's adjustedX property rather than x
// it will be used to re-sort and then assign the x property
void MeshRasterizer::applyGuideCurves(Intercept& icpt, const MorphPosition& morph, bool noOffsetAtEnds) {
    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();
    guideApplier(icpt, morph, noOffsetAtEnds);
}

void MeshRasterizer::cleanUp() {
    clearRasterizationResult(false);
    guideCurveRegions.clear();

    if(! batchMode) {
        makeCopy();
    }

}

void MeshRasterizer::setResolutionIndices(float base) {
    Rasterization::CurveResolutionPolicy::applyResolutionIndices(curves, base, getPaddingSize());
}


MeshRasterizer& MeshRasterizer::operator=(const MeshRasterizer& copy) {
//    jassertfalse;
    this->xMinimum               = copy.xMinimum;
    this->xMaximum               = copy.xMaximum;
    this->mesh                   = copy.mesh;
    this->morph                  = copy.morph;
    this->lowResCurves           = copy.lowResCurves;
    this->scalingType            = copy.scalingType;
    this->name                   = copy.name;
    this->waveform.zeroIndex     = 0;
    this->waveform.oneIndex      = INT_MAX / 2;

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
        MeshRasterizer(copy.name) {
//    jassertfalse;

    operator=(copy);
    initialise();
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

bool MeshRasterizer::hasEnoughCubesForCrossSection() {
    return mesh->getNumCubes() > 1;
}

bool MeshRasterizer::isSampleable() {
    return Rasterization::WaveformSampler::isSampleable(createWaveformView());
}

bool MeshRasterizer::isSampleableAt(float x) {
    return Rasterization::WaveformSampler::isSampleableAt(createWaveformView(), x);
}

void MeshRasterizer::updateBuffers(int size) {
    waveform.place(result.waveformMemory, size);
}

void MeshRasterizer::markWaveformUnsampleable() {
    waveform.waveX.nullify();
    waveform.waveY.nullify();
    unsampleable = true;
}

void MeshRasterizer::clearRasterizationResult(bool clearCurves) {
    markWaveformUnsampleable();

    icpts.clear();
    frontIcpts.clear();
    backIcpts.clear();
    colorPoints.clear();

    if (clearCurves) {
        curves.clear();
    }
}

Rasterization::WaveformBuffers MeshRasterizer::createWaveformView() const {
    return waveform;
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
    source.waveform = waveform;

    Rasterization::RasterizerSnapshotBuilder().publish(rastArrays, source);
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
    randomizeGuideCurveOffsetSeeds(layerSize, tableSize);
}

void MeshRasterizer::randomizeGuideCurveOffsetSeeds(int layerSize, int tableSize) {
    Random rand(Time::currentTimeMillis());
    guideCurveOffsetSeeds.randomize(layerSize, tableSize, rand);
}
