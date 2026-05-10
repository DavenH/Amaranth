#include <iterator>
#include <climits>
#include "GuideCurveProvider.h"
#include "Mesh.h"
#include "MeshRasterizer.h"

#include "VertCube.h"
#include "Rasterization/Policies/Mesh/ComponentGuideSharpnessPolicy.h"
#include "Rasterization/Policies/Curves/CurveWaveformPreparationPolicy.h"
#include "Rasterization/Policies/Mesh/DepthProjectionPolicy.h"
#include "Rasterization/Policies/Mesh/GuideCurvePolicy.h"
#include "Rasterization/Policies/Core/InterceptDegeneracyPolicy.h"
#include "Rasterization/Policies/Core/InterceptSortPolicy.h"
#include "Rasterization/Policies/Curves/InterceptPaddingFlagPolicy.h"
#include "Rasterization/Policies/Core/InterceptRestrictionPolicy.h"
#include "Rasterization/Policies/Core/RasterizerCleanupPolicy.h"
#include "Rasterization/Policies/Core/PaddingPolicy.h"
#include "Rasterization/Policies/Core/PointScalingPolicy.h"
#include "Rasterization/Policies/Core/SnapshotPolicy.h"
#include "Rasterization/Builders/RasterizerSnapshotBuilder.h"
#include "Rasterization/Builders/TransferTable.h"
#include "Rasterization/Interpolation/TrilinearMeshSlicer.h"
#include "Rasterization/Policies/Curves/WaveformBuildPolicy.h"
#include "Rasterization/Pipelines/MeshSlicePipeline.h"
#include "Rasterization/Sampling/GuideCurveSampler.h"
#include "Rasterization/Sampling/WaveformSampler.h"
#include "Rasterization/Sources/MeshCubeSource.h"
#include "../App/AppConstants.h"
#include "../App/MeshLibrary.h"
#include "../Array/ScopedAlloc.h"
#include "../Design/Updating/Updater.h"
#include "../Util/CommonEnums.h"

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

    ,    paddingSize         (2)
    ,    noiseSeed           (-1)
    ,    overridingDim       (Vertex::Time)
    ,    controller()
    ,    meshSlicePipeline()

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

void MeshRasterizer::calcCrossPointsAtTime(float x) {
    morph.time = x;
    calcCrossPoints();
}

void MeshRasterizer::calcCrossPoints() {
    if (controller.calcCrossPoints()) {
        return;
    }

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

    const Rasterization::RenderResult& output = renderMeshSlice(usedMesh, oscPhase);
    publishMeshSliceOutput(output);
    finishCrossPointCalculation();
}

const Rasterization::RenderResult& MeshRasterizer::renderMeshSlice(Mesh* usedMesh, float oscPhase) {
    Rasterization::GuideCurveApplier guideApplier = createGuideCurveApplier();

    return meshSlicePipeline.renderWithReduction(
            Rasterization::MeshCubeSource(usedMesh),
            Rasterization::TrilinearMeshSlicer(),
            createRasterizationRequest(),
            oscPhase,
            guideApplier,
            reduct);
}

bool MeshRasterizer::canRasterizeMesh(Mesh* usedMesh) const {
    return usedMesh != nullptr && usedMesh->getNumCubes() > 0;
}

void MeshRasterizer::beginCrossPointCalculation() {
    icpts.clear();
    preCleanup();

    needsResorting = false;
}

void MeshRasterizer::publishMeshSliceOutput(const Rasterization::RenderResult& output) {
    icpts = output.intercepts;

    if (calcDepthDims) {
        colorPoints = output.colorPoints;
    }
}

void MeshRasterizer::finishCrossPointCalculation() {
    processIntercepts(icpts);
    sortInterceptsIfNeeded();

    if (handleDegenerateInterceptOutput()) {
        return;
    }

    Rasterization::InterceptPaddingFlagPolicy().apply(icpts);

    if (!calcInterceptsOnly) {
        rebuildCurvesFromIntercepts();
    }
}

void MeshRasterizer::processIntercepts(vector<Intercept>& intercepts) {
    controller.processIntercepts(intercepts);
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
    if (controller.hasPrimaryViewDimensionProvider()) {
        return controller.primaryViewDimension();
    }

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
    if (controller.updateCurves()) {
        return;
    }

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

    bool sampleable = Rasterization::WaveformBuildPolicy().build(
            curves,
            context,
            [this](int totalRes) {
                updateBuffers(totalRes);
                return createWaveformRefs();
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
    Rasterization::RasterizerCleanupPolicy().clean(createRasterizerRuntime());
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
    if (controller.pad(intercepts, curves)) {
        return;
    }

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
    Rasterization::RasterizerCleanupOptions options;
    options.clearCurves = false;

    Rasterization::RasterizerCleanupPolicy(options).clean(createRasterizerRuntime());
    guideCurveRegions.clear();

    if(! batchMode) {
        makeCopy();
    }

}

void MeshRasterizer::setResolutionIndices(float base) {
    Rasterization::CurveResolutionPolicy::applyResolutionIndices(curves, base, getPaddingSize());
}


void MeshRasterizer::preCleanup() {
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
    this->controller.resetProviders();
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
    if (controller.hasNumDimensionsProvider()) {
        return controller.numDimensions();
    }

    return 3;
}

bool MeshRasterizer::hasEnoughCubesForCrossSection() {
    if (controller.hasCrossSectionAvailabilityProvider()) {
        return controller.hasEnoughCubesForCrossSection();
    }

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
    waveform.place(result.waveformMemory, size);
}

void MeshRasterizer::markWaveformUnsampleable() {
    Rasterization::RasterizerCleanupPolicy::markWaveformUnsampleable(createRasterizerRuntime());
}

Rasterization::WaveformBuffers MeshRasterizer::createWaveformView() const {
    return waveform;
}

Rasterization::WaveformBufferRefs MeshRasterizer::createWaveformRefs() {
    return Rasterization::WaveformBufferRefs(waveform);
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

Rasterization::GuideCurveApplier MeshRasterizer::createLegacyGuideCurveApplier() {
    return createGuideCurveApplier();
}

void MeshRasterizer::makeCopy() {
    Rasterization::RasterizerSnapshotSource source =
            Rasterization::createRasterizerSnapshotSource(createRasterizerRuntime());
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
    if (controller.updateOffsetSeeds(layerSize, tableSize)) {
        return;
    }

    randomizeGuideCurveOffsetSeeds(layerSize, tableSize);
}

void MeshRasterizer::randomizeGuideCurveOffsetSeeds(int layerSize, int tableSize) {
    Random rand(Time::currentTimeMillis());
    guideCurveOffsetSeeds.randomize(layerSize, tableSize, rand);
}
